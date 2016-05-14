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

#include <query/Operator.h>

#include "AioInputSettings.h"

namespace scidb
{

class LogicalAioInput : public  LogicalOperator
{
public:
    LogicalAioInput(const std::string& logicalName, const std::string& alias):
        LogicalOperator(logicalName, alias)
    {
        ADD_PARAM_VARIES();
    }

    std::vector<shared_ptr<OperatorParamPlaceholder> > nextVaryParamPlaceholder(const std::vector< ArrayDesc> &schemas)
    {
        std::vector<shared_ptr<OperatorParamPlaceholder> > res;
        res.push_back(END_OF_VARIES_PARAMS());
        if (_parameters.size() < AioInputSettings::MAX_PARAMETERS)
        {
            res.push_back(PARAM_CONSTANT("string"));
        }
        return res;
    }

    ArrayDesc inferSchema(std::vector< ArrayDesc> schemas, shared_ptr< Query> query)
    {
        AioInputSettings settings (_parameters, true, query);
        size_t numRequestedAttributes = settings.getNumAttributes();
        size_t requestedChunkSize = settings.getChunkSize();
        size_t const nInstances = query->getInstancesCount();
        vector<DimensionDesc> dimensions(3);
        dimensions[0] = DimensionDesc("tuple_no",           0, 0, CoordinateBounds::getMax(), CoordinateBounds::getMax(), requestedChunkSize, 0);
        dimensions[1] = DimensionDesc("dst_instance_id",    0, 0, nInstances-1, nInstances-1, 1, 0);
        dimensions[2] = DimensionDesc("src_instance_id",    0, 0, nInstances-1, nInstances-1, 1, 0);
        vector<AttributeDesc> attributes;
        if (settings.getSplitOnDimension())
        {   //add 1 for the error column
            dimensions.push_back(DimensionDesc("attribute_no", 0, 0, numRequestedAttributes, numRequestedAttributes, numRequestedAttributes+1, 0));
            attributes.push_back(AttributeDesc(0, "a", TID_STRING, AttributeDesc::IS_NULLABLE, 0));
        }
        else
        {
            for(size_t i=0, n=numRequestedAttributes; i<n; ++i)
            {
                ostringstream attname;
                attname<<"a";
                attname<<i;
                attributes.push_back(AttributeDesc((AttributeID)i, attname.str(),  TID_STRING, AttributeDesc::IS_NULLABLE, 0));
            }
            attributes.push_back(AttributeDesc((AttributeID)numRequestedAttributes, "error", TID_STRING, AttributeDesc::IS_NULLABLE, 0));
        }
        attributes = addEmptyTagAttribute(attributes);
        return ArrayDesc("aio_input", attributes, dimensions, createDistribution(psUndefined), query->getDefaultArrayResidency());
    }
};

REGISTER_LOGICAL_OPERATOR_FACTORY(LogicalAioInput, "aio_input");

} // emd namespace scidb
