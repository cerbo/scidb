########################################
# BEGIN_COPYRIGHT
#
# Copyright (C) 2015-2015 SciDB, Inc.
# All Rights Reserved.
#
# SciDB is free software: you can redistribute it and/or modify
# it under the terms of the AFFERO GNU General Public License as published by
# the Free Software Foundation.
#
# SciDB is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
# INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,
# NON-INFRINGEMENT, OR FITNESS FOR A PARTICULAR PURPOSE. See
# the AFFERO GNU General Public License for the complete license terms.
#
# You should have received a copy of the AFFERO GNU General Public License
# along with SciDB.  If not, see <http://www.gnu.org/licenses/agpl-3.0.html>
#
# END_COPYRIGHT
########################################

# shared library for user defined objects
set (aio_src
Functions.cpp
LogicalAioInput.cpp
LogicalAioSave.cpp
LogicalParse.cpp
LogicalSplit.cpp
PhysicalAioInput.cpp
PhysicalAioSave.cpp
PhysicalParse.cpp
PhysicalSplit.cpp
plugin.cpp
)

file(GLOB aio_include "*.h")
add_library(prototype_aio SHARED ${aio_src} ${aio_include})
extractDebugInfo("${GENERAL_OUTPUT_DIRECTORY}/plugins" "libprototype_aio${CMAKE_SHARED_LIBRARY_SUFFIX}" prototype_aio)
set_target_properties(prototype_aio PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${GENERAL_OUTPUT_DIRECTORY}/plugins)
