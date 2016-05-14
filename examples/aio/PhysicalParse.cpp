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
#include "ParseSettings.h"

#include <boost/algorithm/string.hpp>

using std::make_shared;
using boost::algorithm::is_from_range;

namespace scidb
{

using namespace scidb;


class OutputWriter : public boost::noncopyable
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
    ostringstream _errorBuf;
    Value _buf;

public:
    OutputWriter(ArrayDesc const& schema, shared_ptr<Query>& query, bool splitOnDimension, char const attDelimiter):
        _output(make_shared<MemArray>(schema,query)),
        _outputPosition( splitOnDimension ? 4 : 3, 0),
        _numLiveAttributes(schema.getAttributes(true).size()),
        _outputLineSize(splitOnDimension ? schema.getDimensions()[3].getChunkInterval() : _numLiveAttributes),
        _outputChunkSize(schema.getDimensions()[2].getChunkInterval()),
        _outputArrayIterators(_numLiveAttributes),
        _outputChunkIterators(_numLiveAttributes),
        _splitOnDimension(splitOnDimension),
        _outputColumn(0),
        _attributeDelimiter(attDelimiter)
    {

        for(AttributeID i =0; i<_numLiveAttributes; ++i)
        {
            _outputArrayIterators[i] = _output->getIterator(i);
        }
    }

    void newChunk (Coordinates const& inputChunkPosition, shared_ptr<Query>& query)
    {
        _outputPosition[0] = inputChunkPosition[0];
        _outputPosition[1] = inputChunkPosition[1];
        _outputPosition[2] = 0;
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
        if(_outputColumn < _outputLineSize - 1)
        {
            _buf.setSize(end - start + 1);
            char* d = _buf.getData<char>();
            memcpy(d, start, end-start);
            d[(end-start)]=0;

            //_buf.setString(value);
            if(_splitOnDimension)
            {
                _outputChunkIterators[0] -> setPosition(_outputPosition);
                _outputChunkIterators[0] -> writeItem(_buf);
                ++(_outputPosition[3]);
            }
            else
            {
                _outputChunkIterators[_outputColumn] -> setPosition(_outputPosition);
                _outputChunkIterators[_outputColumn] -> writeItem(_buf);
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
            _buf.setNull();
            if(_splitOnDimension)
            {
                while (_outputColumn < _outputLineSize - 1)
                {
                    _outputChunkIterators[0] -> setPosition(_outputPosition);
                    _outputChunkIterators[0] -> writeItem(_buf);
                    ++(_outputPosition[3]);
                    ++_outputColumn;
                }
            }
            else
            {
                while (_outputColumn < _outputLineSize - 1)
                {
                    _outputChunkIterators[_outputColumn] -> setPosition(_outputPosition);
                    _outputChunkIterators[_outputColumn] -> writeItem(_buf);
                    ++_outputColumn;
                }
            }
            _errorBuf << "short";
        }
        if(_errorBuf.str().size())
        {
            _buf.setString(_errorBuf.str());
        }
        else
        {
            _buf.setNull();
        }
        if(_splitOnDimension)
        {
            _outputChunkIterators[0] -> setPosition(_outputPosition);
            _outputChunkIterators[0] -> writeItem(_buf);
            _outputPosition[3] = 0;
        }
        else
        {
            _outputChunkIterators[_outputLineSize - 1] -> setPosition(_outputPosition);
            _outputChunkIterators[_outputLineSize - 1] -> writeItem(_buf);
        }
        ++(_outputPosition[2]);
        _errorBuf.str("");
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

class PhysicalParse : public PhysicalOperator
{
public:
    PhysicalParse(std::string const& logicalName,
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

    shared_ptr< Array> execute(std::vector< shared_ptr< Array> >& inputArrays, shared_ptr<Query> query)
    {
        ParseSettings settings (_parameters, false, query);
        OutputWriter writer(_schema, query, settings.getSplitOnDimension(), settings.getAttributeDelimiter());
        shared_ptr<Array>& input = inputArrays[0];
        shared_ptr<ConstArrayIterator> inputIterator = input->getConstIterator(0);
        size_t const outputChunkSize = _schema.getDimensions()[2].getChunkInterval();
        char const attDelim = settings.getAttributeDelimiter();
        char const lineDelim = settings.getLineDelimiter();
        while(!inputIterator-> end())
        {
            Coordinates const& pos = inputIterator->getPosition();
            shared_ptr<ConstChunkIterator> inputChunkIterator = inputIterator->getChunk().getConstIterator();
            if(!inputChunkIterator->end()) //just 1 value in chunk
            {
                size_t nLines =0;
                writer.newChunk(inputIterator->getPosition(), query);
                Value const& v = inputChunkIterator->getItem();
                const char *data = v.getString();
                if(data[ v.size() - 1] != 0)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered a string that is not null-terminated; bailing";
                }
                const char* start = data;
                const char* end = start;
                bool finished = false;
                while (!finished)
                {
                    while( (*end)!=attDelim && (*end)!=lineDelim && (*end)!=0)
                    {
                        ++end;
                    }
                    writer.writeValue(start, end);
                    if((*end) == lineDelim || (*end) == 0 )
                    {
                        writer.endLine();
                        ++nLines;
                        if (nLines > outputChunkSize)
                        {
                            throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered a string with more lines than the chunk size; bailing";
                        }
                        if((*end) == 0)
                        {
                            finished = true;
                        }
                    }
                    if ((*end) !=0)
                    {
                        start = end+1;
                        end   = end+1;
                    }
                }
            }
            ++(*inputIterator);
        }
        return writer.finalize();
    }
};

REGISTER_PHYSICAL_OPERATOR_FACTORY(PhysicalParse, "parse", "PhysicalParse");


} // end namespace scidb
