/*
**
* BEGIN_COPYRIGHT
*
* Copyright (C) 2008-2016 SciDB, Inc.
* All Rights Reserved.
*
* accelerated_io_tools is a plugin for SciDB, an Open Source Array DBMS maintained
* by Paradigm4. See http://www.paradigm4.com/
*
* accelerated_io_tools is free software: you can redistribute it and/or modify
* it under the terms of the AFFERO GNU General Public License as published by
* the Free Software Foundation.
*
* accelerated_io_tools is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
* INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,
* NON-INFRINGEMENT, OR FITNESS FOR A PARTICULAR PURPOSE. See
* the AFFERO GNU General Public License for the complete license terms.
*
* You should have received a copy of the AFFERO GNU General Public License
* along with accelerated_io_tools.  If not, see <http://www.gnu.org/licenses/agpl-3.0.html>
*
* END_COPYRIGHT
*/

#include <limits>
#include <limits>
#include <sstream>

#include <boost/unordered_map.hpp>
#include <query/Operator.h>
#include <util/Platform.h>
#include <array/Tile.h>
#include <array/TileIteratorAdaptors.h>
#include <system/Sysinfo.h>
#include <util/Network.h>

#include <boost/algorithm/string.hpp>

#include <fcntl.h>
#include "AioInputSettings.h"

using std::make_shared;

using boost::algorithm::is_from_range;

namespace scidb
{

static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("scidb.alt_load"));

using namespace scidb;

static size_t getChunkOverheadSize()
{
    return             (  sizeof(ConstRLEPayload::Header) +
                                 2 * sizeof(ConstRLEPayload::Segment) +
                                 sizeof(varpart_offset_t) + 5);
}

static size_t getSizeOffset()
{
    return getChunkOverheadSize()-4;
}

class BinEmptySinglePass : public SinglePassArray
{
private:
    typedef SinglePassArray super;
    MemChunk _dummy;

public:
    BinEmptySinglePass(ArrayDesc const& schema):
            super(schema)
    {
        super::setEnforceHorizontalIteration(true);
    }
    virtual ~BinEmptySinglePass()
    {}
    size_t getCurrentRowIndex() const
    {
        return 0;
    }
    bool moveNext(size_t rowIndex)
    {
        return false;
    }
    ConstChunk const& getChunk(AttributeID attr, size_t rowIndex)
    {
        return _dummy;
    }
};

class BinFileSplitArray : public SinglePassArray
{
private:
    typedef SinglePassArray super;
    size_t _rowIndex;
    Address _chunkAddress;
    MemChunk _chunk;
    weak_ptr<Query> _query;
    size_t const _fileBlockSize;
    size_t const _chunkOverheadSize;
    bool         _endOfFile;
    char*        _bufPointer;
    uint32_t*    _sizePointer;
    FILE*        _inputFile;
    size_t const _nInstances;
    ssize_t       _chunkNo;

public:
    BinFileSplitArray(ArrayDesc const& schema,
                      shared_ptr<Query>& query,
                      shared_ptr<AioInputSettings> settings):
        super(schema),
        _rowIndex(0),
        _chunkAddress(0, Coordinates(3,0)),
        _query(query),
        _fileBlockSize(settings->getBlockSize()),
        _chunkOverheadSize( getChunkOverheadSize() ),
        _endOfFile(false),
        _inputFile(NULL),
        _nInstances(query->getInstancesCount()),
        _chunkNo(0)
    {
        super::setEnforceHorizontalIteration(true);
        _chunkAddress.coords[2] = query->getInstanceID();
        try
        {
            _chunk.allocate(_chunkOverheadSize + _fileBlockSize);
        }
        catch(...)
        {
            throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "File splitter cannot allocate memory";
        }
        _bufPointer = (char*) _chunk.getData();
        ConstRLEPayload::Header* hdr = (ConstRLEPayload::Header*) _bufPointer;
        hdr->_magic = RLE_PAYLOAD_MAGIC;
        hdr->_nSegs = 1;
        hdr->_elemSize = 0;
        hdr->_dataSize = _fileBlockSize + 5 + sizeof(varpart_offset_t);
        hdr->_varOffs = sizeof(varpart_offset_t);
        hdr->_isBoolean = 0;
        ConstRLEPayload::Segment* seg = (ConstRLEPayload::Segment*) (hdr+1);
        *seg =  ConstRLEPayload::Segment(0,0,false,false);
        ++seg;
        *seg =  ConstRLEPayload::Segment(1,0,false,false);
        varpart_offset_t* vp =  (varpart_offset_t*) (seg+1);
        *vp = 0;
        uint8_t* sizeFlag = (uint8_t*) (vp+1);
        *sizeFlag =0;
        _sizePointer = (uint32_t*) (sizeFlag + 1);
        *_sizePointer = (uint32_t) _fileBlockSize;
        _bufPointer = (char*) (_sizePointer+1);
        string const& filePath = settings->getInputFilePath();
        _inputFile = fopen(filePath.c_str(), "r");
        if (_inputFile == NULL)
        {
            ostringstream errorMsg;
            errorMsg<<"cannot open file '"<<filePath<<"' on instance "<<query->getInstanceID();
            throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << errorMsg.str().c_str();
        }
        int const header = settings->getHeader();
        if(header>0)
        {
            char *line = NULL;
            size_t linesize = 0;
            ssize_t nread = 0;
            for(int64_t j=0; j<header && nread>=0; ++j)
            {
                nread = getdelim(&line, &linesize, (int)settings->getLineDelimiter(), _inputFile);
            }
            free(line);
        }
    }

    virtual ~BinFileSplitArray()
    {
        if(_inputFile!=NULL)
        {
            ::fclose(_inputFile);
        }
    }

    size_t getCurrentRowIndex() const
    {
        return _rowIndex;
    }

    bool moveNext(size_t rowIndex)
    {
        if(_endOfFile)
        {
            return false;
        }
        size_t numBytes = ::fread(_bufPointer, 1, _fileBlockSize, _inputFile);
        if(numBytes != _fileBlockSize)
        {
            _endOfFile = true;
            ::fclose(_inputFile);
            _inputFile = NULL;
            if(numBytes == 0)
            {
                return false;
            }
            *_sizePointer = (uint32_t) numBytes;
        }
        ++_rowIndex;
        return true;
    }

    ConstChunk const& getChunk(AttributeID attr, size_t rowIndex)
    {
        _chunkAddress.coords[1] = (_rowIndex  - 1 + _chunkAddress.coords[2]) % _nInstances;
        if(_chunkAddress.coords[1] == 0 && _rowIndex > 1)
        {
            _chunkNo++;
        }
        _chunkAddress.coords[0] = _chunkNo;
        shared_ptr<Query> query = Query::getValidQueryPtr(_query);
        _chunk.initialize(this, &super::getArrayDesc(), _chunkAddress, 0);
        return _chunk;
    }
};

class AIOOutputWriter : public boost::noncopyable
{
private:
    shared_ptr<Array> const _output;
    Coordinates _outputPosition;
    size_t const _numLiveAttributes;
    size_t const _outputLineSize;
    size_t const _outputChunkSize;
    vector<shared_ptr<ArrayIterator> > _outputArrayIterators;
    vector<shared_ptr<ChunkIterator> > _outputChunkIterators;
    bool _splitOnDimension;
    size_t _outputColumn;
    char const _attributeDelimiter;
    vector<Value> _buf;
    ostringstream _errorBuf;
    Value _errorBufVal;
    Coordinate _outputPositionLimit;

public:
    AIOOutputWriter(ArrayDesc const& schema, shared_ptr<Query>& query, bool splitOnDimension, char const attDelimiter):
        _output(make_shared<MemArray>(schema,query)),
        _outputPosition( splitOnDimension ? 4 : 3, 0),
        _numLiveAttributes(schema.getAttributes(true).size()),
        _outputLineSize(splitOnDimension ? schema.getDimensions()[3].getChunkInterval() : _numLiveAttributes),
        _outputChunkSize(schema.getDimensions()[0].getChunkInterval()),
        _outputArrayIterators(_numLiveAttributes),
        _outputChunkIterators(_numLiveAttributes),
        _splitOnDimension(splitOnDimension),
        _outputColumn(0),
        _attributeDelimiter(attDelimiter),
        _buf(_outputLineSize-1)
    {
        for(AttributeID i =0; i<_numLiveAttributes; ++i)
        {
            _outputArrayIterators[i] = _output->getIterator(i);
        }
    }

    void newChunk (Coordinates const& inputChunkPosition, shared_ptr<Query>& query)
    {
        _outputPosition[0] = inputChunkPosition[0] * _outputChunkSize;
        _outputPositionLimit = _outputPosition[0] + _outputChunkSize;
        _outputPosition[1] = inputChunkPosition[1];
        _outputPosition[2] = inputChunkPosition[2];
        if(_splitOnDimension)
        {
            _outputPosition[3] = 0;
        }
        for(AttributeID i =0; i<_numLiveAttributes; ++i)
        {
            if(_outputChunkIterators[i].get())
            {
                _outputChunkIterators[i]->flush();
            }
            _outputChunkIterators[i] = _outputArrayIterators[i]->newChunk(_outputPosition).getIterator(query,
                i == 0 ? ChunkIterator::SEQUENTIAL_WRITE : ChunkIterator::SEQUENTIAL_WRITE | ChunkIterator::NO_EMPTY_CHECK);
        }
    }

    void writeValue (char const* start, char const* end)
    {
        if(_outputPosition[0] >= _outputPositionLimit)
        {
            throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "The chunk size is too small for the current block size. Lower the block size or increase chunk size";
        }
        if(_outputColumn < _outputLineSize - 1)
        {
            Value& buf = _buf[_outputColumn];
            buf.setSize(end - start + 1);
            char* d = buf.getData<char>();
            memcpy(d, start, end-start);
            d[(end-start)]=0;
            if(_splitOnDimension)
            {
                _outputChunkIterators[0] -> setPosition(_outputPosition);
                _outputChunkIterators[0] -> writeItem(buf);
                ++(_outputPosition[3]);
            }
            else
            {
                _outputChunkIterators[_outputColumn] -> setPosition(_outputPosition);
                _outputChunkIterators[_outputColumn] -> writeItem(buf);
            }
        }
        else if (_outputColumn == _outputLineSize - 1)
        {
            string value(start, end-start);
            _errorBuf << "long" << _attributeDelimiter << value;
        }
        else
        {
            string value(start, end-start);
            _errorBuf << _attributeDelimiter << value;
        }
        ++_outputColumn;
    }

    void endLine()
    {
        if(_outputColumn < _outputLineSize - 1)
        {
            _errorBufVal.setNull();
            if(_splitOnDimension)
            {
                while (_outputColumn < _outputLineSize - 1)
                {
                    _outputChunkIterators[0] -> setPosition(_outputPosition);
                    _outputChunkIterators[0] -> writeItem(_errorBufVal);
                    ++(_outputPosition[3]);
                    ++_outputColumn;
                }
            }
            else
            {
                while (_outputColumn < _outputLineSize - 1)
                {
                    _outputChunkIterators[_outputColumn] -> setPosition(_outputPosition);
                    _outputChunkIterators[_outputColumn] -> writeItem(_errorBufVal);
                    ++_outputColumn;
                }
            }
            _errorBuf << "short";
        }
        if(_errorBuf.str().size())
        {
            _errorBufVal.setString(_errorBuf.str());
            _errorBuf.str("");
        }
        else
        {
            _errorBufVal.setNull();
        }
        if(_splitOnDimension)
        {
            _outputChunkIterators[0] -> setPosition(_outputPosition);
            _outputChunkIterators[0] -> writeItem(_errorBufVal);
            _outputPosition[3] = 0;
        }
        else
        {
            _outputChunkIterators[_outputLineSize - 1] -> setPosition(_outputPosition);
            _outputChunkIterators[_outputLineSize - 1] -> writeItem(_errorBufVal);
        }
        ++(_outputPosition[0]);
        _outputColumn = 0;
    }

    shared_ptr<Array> finalize()
    {
        for(AttributeID i =0; i<_numLiveAttributes; ++i)
        {
            if(_outputChunkIterators[i].get())
            {
                _outputChunkIterators[i]->flush();
            }
            _outputChunkIterators[i].reset();
            _outputArrayIterators[i].reset();
        }
        return _output;
    }
};

class PhysicalAioInput : public PhysicalOperator
{
public:
    static ArrayDesc getSplitSchema(shared_ptr<Query> & query)
    {
        size_t const nInstances = query->getInstancesCount();
        vector<DimensionDesc> dimensions(3);
        dimensions[0] = DimensionDesc("chunk_no",           0, 0, CoordinateBounds::getMax(), CoordinateBounds::getMax(), 1, 0);
        dimensions[1] = DimensionDesc("dst_instance_id",    0, 0, nInstances-1, nInstances-1, 1, 0);
        dimensions[2] = DimensionDesc("src_instance_id",    0, 0, nInstances-1, nInstances-1, 1, 0);
        vector<AttributeDesc> attributes;
        attributes.push_back(AttributeDesc((AttributeID)0, "value",  TID_BINARY, 0, 0));
        return ArrayDesc("aio_input", attributes, dimensions, defaultPartitioning(), query->getDefaultArrayResidency());
    }

    PhysicalAioInput(std::string const& logicalName,
                     std::string const& physicalName,
                     Parameters const& parameters,
                     ArrayDesc const& schema):
        PhysicalOperator(logicalName, physicalName, parameters, schema)
    {}

    virtual bool changesDistribution(std::vector<ArrayDesc> const&) const
    {
        return true;
    }

    virtual RedistributeContext getOutputDistribution(
            std::vector<RedistributeContext> const& inputDistributions,
            std::vector< ArrayDesc> const& inputSchemas) const
    {
        RedistributeContext distro(_schema.getDistribution(), _schema.getResidency());
        return distro;
    }

    shared_ptr<Array> makeSupplement(shared_ptr<Array>& afterSplit, shared_ptr<Query>& query, shared_ptr<AioInputSettings>& settings, vector<Coordinate>& lastBlocks)
    {
        char const lineDelim = settings->getLineDelimiter();
        shared_ptr<Array> supplement(new MemArray(getSplitSchema(query), query));
        shared_ptr<ConstArrayIterator> srcArrayIter = afterSplit->getConstIterator(0);
        shared_ptr<ArrayIterator> dstArrayIter = supplement->getIterator(0);
        shared_ptr<ChunkIterator> dstChunkIter;
        size_t const nInstances = query->getInstancesCount();
        while(!srcArrayIter->end())
        {
           Coordinates supplementCoords = srcArrayIter->getPosition();
           Coordinate block = supplementCoords[0] * nInstances + supplementCoords[1];
           Coordinate dst   = supplementCoords[1];
           Coordinate src   = supplementCoords[2];
           if(lastBlocks[src] < block)
           {
               lastBlocks[src] = block;
           }
           if(supplementCoords[0] != 0 || supplementCoords[1] != supplementCoords[2])
           {
               ConstChunk const& ch = srcArrayIter->getChunk();
               PinBuffer pinScope(ch);
               char* start = ((char*) ch.getData()) + getChunkOverheadSize();
               uint32_t const sourceSize = *((uint32_t*)(((char*) ch.getData()) + getSizeOffset()));
               if(dst == 0)
               {
                   supplementCoords[1] = nInstances-1;
                   supplementCoords[0]--;
               }
               else
               {
                   supplementCoords[1] = dst - 1;
               }
               char const* lim = start + sourceSize;
               char const* end = start;
               while( end != lim && (*end) != lineDelim)
               {
                   ++end;
               }
               if(end == lim)
               {
                   throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered a whole block without line delimiter characters; Sorry! You need to increase the block size.";
               }
               Value firstLine;
               firstLine.setSize(end-start);
               memcpy(firstLine.data(), start, end-start);
               dstChunkIter = dstArrayIter->newChunk(supplementCoords).getIterator(query,  ChunkIterator::SEQUENTIAL_WRITE);
               dstChunkIter->writeItem(firstLine);
               dstChunkIter->flush();
           }
           ++(*srcArrayIter);
        }
        return supplement;
    }

    void exchangeLastBlocks(vector<Coordinate> &myLastBlocks, shared_ptr<Query>& query)
    {
        InstanceID const myId = query->getInstanceID();
        size_t const numInstances = query->getInstancesCount();
        size_t const vectorSize = numInstances *  sizeof(Coordinate);
        shared_ptr<SharedBuffer> buf(new MemoryBuffer( &(myLastBlocks[0]), vectorSize));
        for(InstanceID i = 0; i<numInstances; ++i)
        {
            if (i == myId)
            {
                continue;
            }
            BufSend(i, buf, query);
        }
        for(InstanceID i = 0; i<numInstances; ++i)
        {
            if (i == myId)
            {
                continue;
            }
            buf = BufReceive(i, query);
            vector<Coordinate> otherLastBlocks(numInstances);
            memcpy(&otherLastBlocks[0], buf->getData(), vectorSize);
            for(size_t j =0; j<numInstances; ++j)
            {
                if(otherLastBlocks[j] > myLastBlocks[j])
                {
                    myLastBlocks[j] = otherLastBlocks[j];
                }
            }
        }
    }

    shared_ptr< Array> execute(std::vector< shared_ptr< Array> >& inputArrays, shared_ptr<Query> query)
    {
        shared_ptr<AioInputSettings> settings (new AioInputSettings(_parameters, false, query));
        shared_ptr<Array> splitData;
        if(settings->thisInstanceReadsData())
        {
            splitData = shared_ptr<BinFileSplitArray>(new BinFileSplitArray(getSplitSchema(query), query, settings));
        }
        else
        {
            splitData = shared_ptr<BinEmptySinglePass>(new BinEmptySinglePass(getSplitSchema(query)));
        }

        splitData = redistributeToRandomAccess(splitData,
                                               createDistribution(psByCol),
                                               ArrayResPtr(),
                                               query);
        size_t const nInstances = query->getInstancesCount();
        vector<Coordinate> lastBlocks(nInstances, -1);
        shared_ptr<Array> supplement = makeSupplement(splitData, query, settings, lastBlocks);
        exchangeLastBlocks(lastBlocks, query);
        supplement = redistributeToRandomAccess(supplement,
                                                createDistribution(psByCol),
                                                ArrayResPtr(),
                                                query);
        shared_ptr<ConstArrayIterator> inputIterator = splitData->getConstIterator(0);
        shared_ptr<ConstArrayIterator> supplementIter = supplement->getConstIterator(0);
        size_t const outputChunkSize = _schema.getDimensions()[0].getChunkInterval();
        char const attDelim = settings->getAttributeDelimiter();
        char const lineDelim = settings->getLineDelimiter();
        AIOOutputWriter writer(_schema, query, settings->getSplitOnDimension(), settings->getAttributeDelimiter());
        size_t const overheadSize = getChunkOverheadSize();
        size_t const sizeOffset = getSizeOffset();
        while(!inputIterator-> end())
        {
            Coordinates const& pos = inputIterator->getPosition();
            Coordinate const block = pos[0] * nInstances + pos[1];
            bool const lastBlock = (lastBlocks[ pos[2] ] == block);
            ConstChunk const& chunk =  inputIterator->getChunk();
            PinBuffer pinScope(chunk);
            char* chunkData = ((char*) chunk.getData());
            char* sourceStart = chunkData + overheadSize;
            char* chunkBodyStart  = sourceStart;
            uint32_t sourceSize = *((uint32_t*)(chunkData + sizeOffset));
            if(sourceSize == 0)
            {
                throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "[defensive] encountered a chunk with no data.";
            }
            size_t nLines =0;
            if(pos[0] != 0 || pos[1] != pos[2])
            {
                while((*sourceStart)!=lineDelim)
                {
                    sourceStart ++;
                }
                sourceStart ++;
                sourceSize = sourceSize - (sourceStart - chunkBodyStart);
            }
            bool haveSupplement = supplementIter->setPosition(pos);
            vector<char>buf;
            if(haveSupplement)
            {
                shared_ptr<ConstChunkIterator> supplementChunkIterator = supplementIter->getChunk().getConstIterator();
                Value const &s = supplementChunkIterator->getItem();
                buf.resize(sourceSize+s.size());
                memcpy(&buf[0], sourceStart, sourceSize);
                memcpy(&buf[0]+ sourceSize, s.data(), s.size());
            }
            else
            {
                buf.resize(sourceSize);
                memcpy(&buf[0], sourceStart, sourceSize);
            }
            if(lastBlock && buf.size() <= 1)
            {
                ++(*inputIterator);
                continue;
            }
            const char *data = &buf[0];
            const char* start = data;
            const char* end = start;
            const char* terminus = start + buf.size();
            bool finished = false;
            writer.newChunk(pos, query);
            while (!finished)
            {
                while( end != terminus && (*end)!=attDelim && (*end)!=lineDelim )
                {
                    ++end;
                }
                writer.writeValue(start, end);
                if(end == terminus || (*end) == lineDelim )
                {
                    writer.endLine();
                    ++nLines;
                    if (nLines > outputChunkSize)
                    {
                        throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered a string with more lines than the chunk size; bailing";
                    }
                    if(end == terminus || (lastBlock && end == terminus-1))
                    {
                        finished = true;
                    }
                }
                if (end != terminus)
                {
                    start = end+1;
                    end   = end+1;
                }
            }
            ++(*inputIterator);
        }
        return writer.finalize();
    }
};

REGISTER_PHYSICAL_OPERATOR_FACTORY(PhysicalAioInput, "aio_input", "PhysicalAioInput");

} // end namespace scidb
