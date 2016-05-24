/*
**
* BEGIN_COPYRIGHT
*
* Copyright (C) 2008-2014 SciDB, Inc.
*
* SciDB is free software: you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation version 3 of the License.
*
* This software is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
* INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, NON-INFRINGEMENT, OR
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for the
* complete license terms.
*
* You should have received a copy of the GNU General Public License
* along with SciDB.  If not, see <http://www.gnu.org/licenses/>.
*
* END_COPYRIGHT
*/


/* Minimal SciDB Client Interface */
#include <string>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <algorithm>

#define MAX_VARLEN 4096

#include "SciDBAPI.h"
using namespace std;
using namespace scidb;

#include <array/StreamArray.h>
#include <boost/bind.hpp>
#include <fstream>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/logger.h>
#include <memory>
#include <network/Connection.h>
#include <network/MessageUtils.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <regex>
#include <stdlib.h>
#include <string>
#include <system/Exceptions.h>
#include <system/ErrorCodes.h>
#include <util/AuthenticationFile.h>
#include <util/ConfigUser.h>
#include <util/PluginManager.h>
#include <util/Singleton.h>

/// @param[in]  connection  the result of scidb::getSciDB().connect(host, port).
/// @param[in]  name  the user name.
/// @param[in]  hashedPassword  the hashed password. See _hashPassword() in SciDBRemote.cpp.
/// @param[out] errorMessage  placeholder for the error message, in case of failure.
/// @return whether the new client started and got authenticated.
int myNewClientStart(
    void*const connection,
    const char *name,
    const char *hashedPassword,
    std::string*const errorMessage)
{
  scidb::BaseConnection *baseConnection =
        static_cast<scidb::BaseConnection*>(connection);

  // --- send newClientStart message --- //
  std::shared_ptr<scidb::MessageDesc> msgNewClientStart =
    std::make_shared<scidb::MessageDesc>(scidb::mtNewClientStart);

  std::shared_ptr<scidb::MessageDesc> resultMessage =
    baseConnection->sendAndReadMessage<scidb::MessageDesc>(msgNewClientStart);

    while (true) {
        switch(resultMessage->getMessageType()) {
    case scidb::mtSecurityMessage:
    {
      std::string strMessage = resultMessage->getRecord<scidb_msg::SecurityMessage>()->msg();
      transform(strMessage.begin(), strMessage.end(),  strMessage.begin(),  ::tolower );

      bool requestingName = true;
      if(strMessage.compare("login:") == 0) {
        requestingName = true;
      } else if(strMessage.compare("password:") == 0) {
        requestingName = false;
      } else {
        *errorMessage = "Unknown server request - " + strMessage;
        return 0;
      }
      std::shared_ptr<scidb::MessageDesc> msgSecurityMessageResponse =
        std::make_shared<scidb::MessageDesc>(scidb::mtSecurityMessageResponse);
      msgSecurityMessageResponse->getRecord<scidb_msg::SecurityMessageResponse>()->
        set_response(requestingName ? name : hashedPassword);
      resultMessage =  baseConnection->sendAndReadMessage<scidb::MessageDesc>(msgSecurityMessageResponse);
      break;
    }
    case scidb::mtNewClientComplete:
      if (resultMessage->getRecord<scidb_msg::NewClientComplete>()->authenticated()) {
        return 1;
      } else {
        *errorMessage = "Failed to authenticate.";
        return 0;
      }
      break;

    case scidb::mtError:
    {
      std::shared_ptr<scidb_msg::Error> error = resultMessage->getRecord<scidb_msg::Error>();
      if (error->short_error_code() != SCIDB_E_NO_ERROR) {
        *errorMessage = error->function();
        return 0;
      }
      break;
    }

    default:
      *errorMessage = "SciDBRemote::newClientStart unknown messageType=" + resultMessage->getMessageType();
      return 0;
    }
  }
}

/* This structure mirrors the 'QueryID' object data in SciDB 15.7 and later */
typedef struct queryid
{
  unsigned long long coordinatorid;
  unsigned long long queryid;
} sQueryID;

/* This structure is used to hand off a SciDB QueryResult object back into
 * C functions.
 */
struct prep
{
  sQueryID queryid;
  void *queryresult;
};

/* Authenticate a SciDB connection with a user name and
 * password. Return the connection or NULL if fail.
 */
extern "C" void * scidbauth(void *con, const char *name, const char *password)
{
  const scidb::SciDB& db = scidb::getSciDB();
  std::string errorMessage;
  try{
    if (!myNewClientStart(con, name, password, &errorMessage))
    {
      db.disconnect(con);
      con = NULL;
    }
  } catch(std::exception& e)
  {
    db.disconnect(con);
    con = NULL;
  }
  return con;
}

/* Connect to a SciDB instance on the specified host and port.
 * Returns a pointer to the SciDB connection context, or NULL
 * if an error occurred.
 */
extern "C" void * scidbconnect(const char *host, int port)
{
  void* conn = NULL;
  const scidb::SciDB& db = scidb::getSciDB();
  try{
    conn = (void *)db.connect(host, port);
  } catch(std::exception& e)
  {
    conn=NULL;
  }
  return conn;
}

/* Disconnect a connected SciDB client connection,
 * the con pointer should be from the scidbconnect function above.
 * Does not return anything, but we should at least report errors XXX fix.
 */
extern "C" void scidbdisconnect(void * con)
{
  const scidb::SciDB& db = scidb::getSciDB();
  try{
    db.disconnect((void *)con);
  } catch(std::exception& e)
  {
  }
}

/* Execute a query, using the indicated SciDB client connection.
 * con SciDB connection context
 * query A query string to execute
 * afl = 0 use AQL otherwise afl
 * err place any character string errors that occur here
 * Returns a SciDB query ID on success, or zero on error (and writes the
 *  text of the error to the character buffer err).
 */
extern "C" unsigned long long
executeQuery(void *con, char *query, int afl, char *err)
{
  unsigned long long id = 0;
  const scidb::SciDB& db = scidb::getSciDB();
  const string &queryString = (const char *)query;
  scidb::QueryResult queryResult;
  try{
    db.executeQuery(queryString, bool(afl), queryResult, (void *)con);
    id = (unsigned long long)queryResult.queryID.getId();
  } catch(std::exception& e)
  {
    snprintf(err,MAX_VARLEN,"%s",e.what());
  }
  return id;
}

/* Prepare a query using the indicated SciDB client connection.
 * result: Pointer to a struct prep, filled in by this function on success
 * con: a scidb connection context
 * afl = 0 use AQL, otherwise AFL.
 * err: a buff of length MAX_VARLEN to hold  error string should one occur
 *
 * Upon returning, either the results structure is populated with a non-NULL
 * queryresult pointer and a valid queryid and the err string is NULL, or
 * ther err string is not NULL and the queryresult is NULL.
 */
extern "C" void
prepare_query(void *result, void *con, char *query, int afl, char *err)
{
  struct prep *p;
  unsigned long long id = -1;
  const scidb::SciDB& db = scidb::getSciDB();
  const string &queryString = (const char *)query;
  scidb::QueryResult *q = new scidb::QueryResult();
  if(!q) 
  {
    snprintf(err,MAX_VARLEN,"Unable to allocate query result\n");
    return;
  }
  p = (struct prep *)result;
  try
  {
    db.prepareQuery(queryString, bool(afl), "", *q, con);
    p->queryresult = (void *)q;
    p->queryid.queryid = (unsigned long long)q->queryID.getId();
    p->queryid.coordinatorid = (unsigned long long)q->queryID.getCoordinatorId();
  } catch(std::exception& e)
  {
    delete q;
    p->queryresult = NULL;
    snprintf(err,MAX_VARLEN,"%s",e.what());
  }
  return;
}

/* Execute a prepared requery stored in the prep structure pq on the scidb
 * connection context con. Set afl to 0 for AQL query, to 1 for AFL query. The
 * char buffer err is a buffer of length MAX_VARLEN on input that will hold an
 * error string on output, should one occur. The queryresult object pointed to
 * from within pq is de-allocated by this function.  Successful exit returns
 * a sQueryID struct.  Failure populates the err buffer with an error string and
 * returns a sQueryID struct with queryid set to 0.
 */
extern "C" sQueryID
execute_prepared_query(void *con, char *query, struct prep *pq, int afl, char *err)
{
  sQueryID qid;
  const string &queryString = (const char *)query;
  const scidb::SciDB& db = scidb::getSciDB();
  qid.queryid = 0;
  qid.coordinatorid = 0;
  scidb::QueryResult *q = (scidb::QueryResult *)pq->queryresult;
  if(!q)
  {
    snprintf(err,MAX_VARLEN,"Invalid query result object.\n");
    return qid;
  }
  try{
    db.executeQuery(queryString, bool(afl), *q, (void *)con);
    qid.queryid = pq->queryid.queryid;
    qid.coordinatorid = pq->queryid.coordinatorid;
  } catch(std::exception& e)
  {
    qid.queryid = 0;
    snprintf(err,MAX_VARLEN,"%s",e.what());
  }
  delete q;
  pq->queryresult = NULL;
  return qid;
}


/* Complete a SciDB query, where char buffer err is a buffer of length
 * MAX_VARLEN on input that will hold an error message should one occur.
 */
extern "C" void completeQuery(sQueryID qid, void *con, char *err)
{
  const scidb::SciDB& db = scidb::getSciDB();
  scidb::QueryID q = scidb::QueryID(qid.coordinatorid, qid.queryid);
  try{
    db.completeQuery(q, (void *)con);
  } catch(std::exception& e)
  {
    snprintf(err,MAX_VARLEN,"%s",e.what());
  }
}
