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

#ifndef SRC_UNPARSETEMPLATEPARSER_H_
#define SRC_UNPARSETEMPLATEPARSER_H_

#include <query/TypeSystem.h>
#include <query/FunctionLibrary.h>

#include <string>
#include <vector>
#include <ctype.h>

namespace scidb
{
    using namespace std;

    class TemplateScanner
    {
        std::string format;
        std::string ident;
        size_t pos;
        int    num;

      public:
        enum Token {
            TKN_EOF,
            TKN_IDENT,
            TKN_LPAR,
            TKN_RPAR,
            TKN_COMMA,
            TKN_NUMBER
        };

        std::string const& getIdent() const
        {
            return ident;
        }

        int getNumber() const
        {
            return num;
        }

        size_t getPosition() const
        {
            return pos;
        }

        TemplateScanner(string const& fmt) : format(fmt), pos(0) {}

        Token get()
        {
            int ch = 0;

            while (pos < format.size() && isspace(ch = format[pos])) {
                pos += 1; // ignore whitespaces
            }
            if (pos == format.size()) {
                return TKN_EOF;
            }

            switch (ch) {
              case '(':
                pos += 1;
                return TKN_LPAR;
              case ')':
                pos += 1;
                return TKN_RPAR;
              case ',':
                pos += 1;
                return TKN_COMMA;
              default:
                if (isdigit(ch)) {
                    num = 0;
                    do {
                        pos += 1;
                        num = num*10 + ch - '0';
                    } while (pos < format.size() && isdigit(ch = format[pos]));
                    return TKN_NUMBER;
                } else if (isalpha(ch)) {
                    ident.clear();
                    do {
                        pos += 1;
                        ident += (char)ch;
                    } while (pos < format.size() && (isalnum(ch = format[pos]) || ch == '_'));
                    return TKN_IDENT;
                } else {
                    throw USER_EXCEPTION(SCIDB_SE_EXECUTION, SCIDB_LE_TEMPLATE_PARSE_ERROR) << pos;
                }
            }
        }
    };

    struct ExchangeTemplate
    {
        struct Column {
            bool skip;
            bool nullable;
            Type internalType;
            Type externalType;
            size_t fixedSize;
            FunctionPointer converter;
        };
        std::vector<Column> columns;
        bool opaque;
    };

    class TemplateParser
    {
      public:
        static ExchangeTemplate parse(ArrayDesc const& desc, std::string const& format, bool isImport);
    };
}


#endif /* SRC_UNPARSETEMPLATEPARSER_H_ */
