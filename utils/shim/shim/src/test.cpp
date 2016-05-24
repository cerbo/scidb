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
#include "SciDBAPI.h"
#include <stdlib.h>
#include <string>
#include <system/Exceptions.h>
#include <system/ErrorCodes.h>
#include <util/AuthenticationFile.h>
#include <util/ConfigUser.h>
#include <util/PluginManager.h>
#include <util/Singleton.h>

using namespace scidb;
using namespace std;

/// @param[in]  connection  the result of scidb::getSciDB().connect(host, port).
/// @param[in]  name  the user name.
/// @param[in]  hashedPassword  the hashed password. See _hashPassword() in SciDBRemote.cpp.
/// @param[out] errorMessage  placeholder for the error message, in case of failure.
/// @return whether the new client started and got authenticated.
bool myNewClientStart(
    void*const connection,
    const std::string name,
    const std::string hashedPassword,
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
			transform(strMessage.begin(), strMessage.end(),	strMessage.begin(),	::tolower );

			bool requestingName = true;
			if(strMessage.compare("login:") == 0) {
				requestingName = true;
			} else if(strMessage.compare("password:") == 0) {
				requestingName = false;
			} else {
				*errorMessage = "Unknown server request - " + strMessage;
				return false;
			}
			std::shared_ptr<scidb::MessageDesc> msgSecurityMessageResponse =
				std::make_shared<scidb::MessageDesc>(scidb::mtSecurityMessageResponse);
			msgSecurityMessageResponse->getRecord<scidb_msg::SecurityMessageResponse>()->
				set_response(requestingName ? name.c_str() : hashedPassword.c_str());
			resultMessage =	baseConnection->sendAndReadMessage<scidb::MessageDesc>(msgSecurityMessageResponse);
			break;
		}
		case scidb::mtNewClientComplete:
			if (resultMessage->getRecord<scidb_msg::NewClientComplete>()->authenticated()) {
				return true;
			} else {
				*errorMessage = "Failed to authenticate.";
				return false;
			}
			break;

		case scidb::mtError:
		{
			std::shared_ptr<scidb_msg::Error> error = resultMessage->getRecord<scidb_msg::Error>();
			if (error->short_error_code() != SCIDB_E_NO_ERROR) {
				*errorMessage = error->function();
				return false;
			}
			break;
		}

		default:
			*errorMessage = "SciDBRemote::newClientStart unknown messageType=" + resultMessage->getMessageType();
			return false;
        }
    }
}

int main()
{
    const scidb::SciDB& sciDB = scidb::getSciDB();
    void* connection = sciDB.connect(
        "localhost",
        1239);

    const std::string username = "root";
    const std::string hashedPassword = "eUCUk3B57IVO9ZfJB6CIEHl/0lxrWg/7PV8KytUNY6kPLhTX2db48GHGHoizKyH+uGkCfNTYZrJgKzjWOhjuvg==";
    std::string errorMessage;

    if (myNewClientStart(connection, username, hashedPassword, &errorMessage)) {
    	cout << "Connected." << endl;
    } else {
    	cout << "Failed to connect: " << errorMessage << endl;
    }
}
