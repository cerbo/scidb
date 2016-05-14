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

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <query/Operator.h>

#ifndef SPLIT_SETTINGS
#define SPLIT_SETTINGS

using boost::starts_with;
using boost::lexical_cast;
using boost::bad_lexical_cast;
using namespace boost::filesystem;
using namespace std;

namespace scidb
{
class SplitSettings
{
private:

    bool             _singlepath;
    bool             _multiplepath;
    string           _inputFilePath;
    vector<string>   _inputPaths;
    vector<int64_t>  _inputInstances;
    int64_t          _instanceParse;
    int64_t          _linesPerChunk;
    bool             _linesPerChunkSet;
    int64_t          _bufferSize;
    bool             _bufferSizeSet;
    char             _delimiter;
    bool             _delimiterSet;
    int64_t          _header;
    bool             _headerSet;

public:
    static const size_t MAX_PARAMETERS = 7;

    SplitSettings(vector<shared_ptr<OperatorParam> > const& operatorParameters,
                 bool logical,
                 shared_ptr<Query>& query):
       _singlepath(false),
       _multiplepath(false),
       _inputFilePath(""),
       _instanceParse(-1),
       _linesPerChunk(1000000),
       _linesPerChunkSet(false),
       _bufferSize(10*1024*1024),
       _bufferSizeSet(false),
       _delimiter('\n'),
       _delimiterSet(false),
       _header(0),
       _headerSet(false)
    {
        string const inputFilePathHeader        = "input_file_path=";
        string const inputPathsHeader           = "paths=";
        string const inputInstancesHeader       = "instances=";
        string const linesPerChunkHeader        = "lines_per_chunk=";
        string const bufferSizeHeader           = "buffer_size=";
        string const delimiterHeader            = "delimiter=";
        string const headerHeader               = "header=";

        int64_t const myInstanceId = query->getInstanceID();
        size_t const nParams = operatorParameters.size();
        if (nParams > MAX_PARAMETERS)
        {   //assert-like exception. Caller should have taken care of this!
            throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal number of parameters passed to SplitSettings";
        }
        for (size_t i= 0; i<nParams; ++i)
        {
            shared_ptr<OperatorParam>const& param = operatorParameters[i];
            string parameterString;
            if (logical)
            {
                parameterString = evaluate(((shared_ptr<OperatorParamLogicalExpression>&) param)->getExpression(),query, TID_STRING).getString();
            }
            else
            {
                parameterString = ((shared_ptr<OperatorParamPhysicalExpression>&) param)->getExpression()->evaluate().getString();
            }
            if      (starts_with(parameterString, inputFilePathHeader))
            {
                if (_inputFilePath != "")
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal attempt to set the input file path multiple times";
                }
                string paramContent = parameterString.substr(inputFilePathHeader.size());
                boost::algorithm::trim(paramContent);
                _singlepath = true;
                _inputFilePath = paramContent;
                _instanceParse = 0;
            }
            else if  (starts_with(parameterString, inputPathsHeader))
            {
                if (_inputPaths.size() > 0)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal attempt to set the input file paths multiple times";
                }
                string paramContent = parameterString.substr(inputPathsHeader.size());
                boost::algorithm::trim(paramContent);
                char delimiter=';';
                vector<string> internal;
                stringstream ss(paramContent); // Turn the string into a stream.
                string tok;
                _multiplepath = true;
                while(getline(ss, tok, delimiter)) 
                {
                    _inputPaths.push_back(tok);
                }
            }
            else if  (starts_with(parameterString, inputInstancesHeader))
            {
                if (_inputInstances.size() > 0)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal attempt to set the input instances multiple times";
                }
                string paramContent = parameterString.substr(inputInstancesHeader.size());
                boost::algorithm::trim(paramContent);
                char delimiter=';';
                vector<string> internal;
                stringstream ss(paramContent); // Turn the string into a stream.
                string tok;
                while(getline(ss, tok, delimiter)) 
                {
                    _inputInstances.push_back(lexical_cast<int64_t>(tok));
                }
            }
            else if (starts_with(parameterString, headerHeader))
            {
                if (_headerSet)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal attempt to set the header multiple times";
                }
                string paramContent = parameterString.substr(headerHeader.size());
                boost::algorithm::trim(paramContent);
                _header = lexical_cast<int64_t>(paramContent);
                _headerSet = true;
            }
            else if (starts_with(parameterString, linesPerChunkHeader))  //yeah, yeah it's a long function...
            {
                if (_linesPerChunkSet)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal attempt to set lines_per_chunk multiple times";
                }
                string paramContent = parameterString.substr(linesPerChunkHeader.size());
                boost::algorithm::trim(paramContent);
                try
                {
                    _linesPerChunk = lexical_cast<int64_t>(paramContent);
                    if(_linesPerChunk <=0 )
                    {
                        throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "lines_per_chunk must be positive";
                    }
                    _linesPerChunkSet = true;
                }
                catch (bad_lexical_cast const& exn)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "could not parse lines_per_chunk";
                }
            }
            else if (starts_with(parameterString, bufferSizeHeader))
            {
                if (_bufferSizeSet)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal attempt to set buffer_size multiple times";
                }
                string paramContent = parameterString.substr(bufferSizeHeader.size());
                boost::algorithm::trim(paramContent);
                try
                {
                    _bufferSize = lexical_cast<int64_t>(paramContent);
                    if(_bufferSize<=0)
                    {
                        throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "lines_per_chunk must be positive";
                    }
                    _bufferSizeSet = true;
                }
                catch (bad_lexical_cast const& exn)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "could not parse buffer_size";
                }
            }
            else if (starts_with(parameterString, delimiterHeader))
            {
                if(_delimiterSet)
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal attempt to set delimiter multiple times";
                }
                string paramContent = parameterString.substr(delimiterHeader.size());
                boost::algorithm::trim(paramContent);
                if (paramContent == "\\t")
                {
                    _delimiter = '\t';
                }
                else if (paramContent == "\\r")
                {
                    _delimiter = '\r';
                }
                else if (paramContent == "\\n")
                {
                    _delimiter = '\n';
                }
                else if (paramContent == "")
                {
                    _delimiter = ' ';
                }
                else
                {
                    try
                    {
                       _delimiter = lexical_cast<char>(paramContent);
                    }
                    catch (bad_lexical_cast const& exn)
                    {
                       throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "could not parse delimiter";
                    }
                }
                _delimiterSet = true;
            }
            else
            {
                if (_inputFilePath != "")
                {
                   throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "illegal attempt to set the input file path multiple times";
                }
                string path = parameterString;
                boost::algorithm::trim(path);
                _singlepath     = true;
                _inputFilePath  = path;
                _instanceParse  = 0;
            }
        }

        if(_multiplepath)
        {
            if(_inputInstances.size() != _inputPaths.size())
            {
                throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Number of paths do not equal the number of instances.";
            }
            std::set<string>uniqueInstances;
            std::set<int64_t> s(_inputInstances.begin(), _inputInstances.end()); 
            if(s.size() !=  _inputPaths.size())
            {
               throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Input instances were not unique.";
            }
            if (_singlepath == true)
            {
                throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Both single path and multiple path were declared.";
            }
            std::vector<int64_t>::iterator itrel = std::find(_inputInstances.begin(), _inputInstances.end(), -1);
            if((_inputInstances.size()==1) && (itrel != _inputInstances.end()))
            {
                 string relinputpath  = _inputPaths[0];
                 _inputFilePath  = relinputpath;  
                 _instanceParse  = myInstanceId;
            }
            else
            {
                int const numinstances = query->getInstancesCount()-1;
                if (std::find_if (_inputInstances.begin(), _inputInstances.end() , bind2nd(greater<int64_t>(),numinstances)) != _inputInstances.end())
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "instance specified that is greater than numinstances";
                }
                if (std::find_if (_inputInstances.begin(), _inputInstances.end() , bind2nd(less<int64_t>(),0)) != _inputInstances.end())
                {
                    throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "instance specified that is less than 0";
                }
                std::vector<int64_t>::iterator it = std::find(_inputInstances.begin(), _inputInstances.end(), myInstanceId);
                if (it != _inputInstances.end())
                {
                    int64_t index = std::distance(_inputInstances.begin(), it);
                    _instanceParse       = myInstanceId;
                    _inputFilePath       = _inputPaths[index];
                }
            }
        }
        else if (_inputInstances.size() > 0)
        {
            throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "The instances argument is not used with a single file path";
        }
    }

    int64_t const& getParseInstance() const
    {
        return _instanceParse;
    }

    string const& getInputFilePath() const
    {
        return _inputFilePath;
    }

    size_t getLinesPerChunk() const
    {
        return _linesPerChunk;
    }

    size_t getBufferSize() const
    {
        return _bufferSize;
    }

    char getDelimiter() const
    {
        return _delimiter;
    }

    int64_t getHeader() const
    {
        return _header;
    }
};

}

#endif //SPLIT_SETTINGS
