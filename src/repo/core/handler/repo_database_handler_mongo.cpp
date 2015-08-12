/**
*  Copyright (C) 2015 3D Repo Ltd
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU Affero General Public License as
*  published by the Free Software Foundation, either version 3 of the
*  License, or (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Affero General Public License for more details.
*
*  You should have received a copy of the GNU Affero General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
*  Mongo database handler
*/

#include <regex>

#include "repo_database_handler_mongo.h"

using namespace repo::core::handler;

static uint64_t MAX_MONGO_BSON_SIZE=16777216L;
//------------------------------------------------------------------------------

const std::string repo::core::handler::MongoDatabaseHandler::SYSTEM_ROLES_COLLECTION = "system.roles";
const std::list<std::string> repo::core::handler::MongoDatabaseHandler::ANY_DATABASE_ROLES =
{ "dbAdmin", "dbOwner", "read", "readWrite", "userAdmin" };
const std::list<std::string> repo::core::handler::MongoDatabaseHandler::ADMIN_ONLY_DATABASE_ROLES =
{ "backup", "clusterAdmin", "clusterManager", "clusterMonitor", "dbAdminAnyDatabase",
"hostManager", "readAnyDatabase", "readWriteAnyDatabase", "restore", "root",
"userAdminAnyDatabase" };

const std::string repo::core::handler::MongoDatabaseHandler::AUTH_MECH = "MONGODB-CR";
//------------------------------------------------------------------------------

MongoDatabaseHandler* MongoDatabaseHandler::handler = NULL;

MongoDatabaseHandler::MongoDatabaseHandler(
	const mongo::ConnectionString &dbAddress,
	const uint32_t                &maxConnections,
	const std::string             &dbName,
	const std::string             &username,
	const std::string             &password,
	const bool                    &pwDigested) :
	AbstractDatabaseHandler(MAX_MONGO_BSON_SIZE)
{
	mongo::client::initialize();
	workerPool = new connectionPool::MongoConnectionPool(maxConnections, dbAddress, createAuthBSON(dbName, username, password, pwDigested));
}

/**
* A Deconstructor
*/
MongoDatabaseHandler::~MongoDatabaseHandler()
{
	if (workerPool)
		delete workerPool;
}

bool MongoDatabaseHandler::caseInsensitiveStringCompare(
	const std::string& s1,
	const std::string& s2)
{
	return strcasecmp(s1.c_str(), s2.c_str()) <= 0;
}

uint64_t MongoDatabaseHandler::countItemsInCollection(
	const std::string &database,
	const std::string &collection,
	std::string &errMsg)
{
	uint64_t numItems;
	mongo::DBClientBase *worker;
	try{

		worker = workerPool->getWorker();
		numItems = worker->count(database + "." + collection);
	}
	catch (mongo::DBException& e)
	{
		errMsg =  "Failed to count num. items within "
			+ database + "." + collection + ":" + e.what();
		BOOST_LOG_TRIVIAL(error) << errMsg;
	}

	workerPool->returnWorker(worker);

	return numItems;

}

mongo::BSONObj* MongoDatabaseHandler::createAuthBSON(
	const std::string &database,
	const std::string &username, 
	const std::string &password,
	const bool        &pwDigested)
{
	mongo::BSONObj* authBson = 0;
	if (!username.empty())
	{
		std::string passwordDigest = pwDigested ?
		password : mongo::DBClientWithCommands::createPasswordDigest(username, password);
		authBson = new mongo::BSONObj(BSON("user" << username <<
			"db" << database <<
			"pwd" << passwordDigest <<
			"digestPassword" << false <<
			"mechanism" << AUTH_MECH));
	}

	return authBson;

		
}

bool MongoDatabaseHandler::dropCollection(
	const std::string &database,
	const std::string &collection,
	std::string &errMsg)
{

	bool success = true;
	mongo::DBClientBase *worker;
	try{

		worker = workerPool->getWorker();
		worker->dropCollection(database + "." + collection);
	}
	catch (mongo::DBException& e)
	{
		BOOST_LOG_TRIVIAL(error) << "Failed to drop collection ("
			<< database << "." << collection << ":" << e.what();
	}

	workerPool->returnWorker(worker);

	return true;
}

bool MongoDatabaseHandler::dropDatabase(
	const std::string &database,
	std::string &errMsg)
{
	bool success = true;
	mongo::DBClientBase *worker;
	try{

		worker = workerPool->getWorker();
		worker->dropDatabase(database);
	}
	catch (mongo::DBException& e)
	{
		BOOST_LOG_TRIVIAL(error) << "Failed to drop database :" << e.what();
	}

	workerPool->returnWorker(worker);

	return true;
}

mongo::BSONObj MongoDatabaseHandler::fieldsToReturn(
	const std::list<std::string>& fields,
	bool excludeIdField)
{
	mongo::BSONObjBuilder fieldsToReturn;
	std::list<std::string>::const_iterator it;
	for (it = fields.begin(); it != fields.end(); ++it)
	{
		fieldsToReturn << *it << 1;
		excludeIdField = excludeIdField && ID != *it;
	}
	if (excludeIdField)
		fieldsToReturn << ID << 0;

	return fieldsToReturn.obj();
}

std::vector<repo::core::model::bson::RepoBSON> MongoDatabaseHandler::findAllByUniqueIDs(
	const std::string& database,
	const std::string& collection,
	const repo::core::model::bson::RepoBSON& uuids){

	std::vector<repo::core::model::bson::RepoBSON> data;

	mongo::BSONArray array = mongo::BSONArray(uuids);

	int fieldsCount = array.nFields();
	if (fieldsCount > 0)
	{
		mongo::DBClientBase *worker;
		try{
			unsigned long long retrieved = 0;
			std::auto_ptr<mongo::DBClientCursor> cursor;
			worker = workerPool->getWorker();
			do
			{

				mongo::BSONObjBuilder query;
				query << ID << BSON("$in" << array);


				cursor = worker->query(
					database + "." + collection,
					query.obj(),
					0,
					retrieved);

				for (; cursor.get() && cursor->more(); ++retrieved)
				{
					data.push_back(repo::core::model::bson::RepoBSON(cursor->nextSafe().copy()));
				}
			} while (cursor.get() && cursor->more());

			if (fieldsCount != retrieved){
				BOOST_LOG_TRIVIAL(error) << "Number of documents("<< retrieved<<") retreived by findAllByUniqueIDs did not match the number of unique IDs(" <<  fieldsCount <<")!";
			}
		}
		catch (mongo::DBException& e)
		{
			BOOST_LOG_TRIVIAL(error) << e.what();
		}

		workerPool->returnWorker(worker);
	}



	return data;
}

repo::core::model::bson::RepoBSON MongoDatabaseHandler::findOneBySharedID(
	const std::string& database,
	const std::string& collection,
	const repoUUID& uuid,
	const std::string& sortField)
{

	repo::core::model::bson::RepoBSON bson;
	mongo::DBClientBase *worker;
	try
	{
		repo::core::model::bson::RepoBSONBuilder queryBuilder;
		queryBuilder.append("shared_id", uuid);
		//----------------------------------------------------------------------
	
		worker = workerPool->getWorker();
		mongo::BSONObj bsonMongo = worker->findOne(
			getNamespace(database, collection),
			mongo::Query(queryBuilder.obj()).sort(sortField, -1));

		bson = repo::core::model::bson::RepoBSON(bsonMongo);
	}
	catch (mongo::DBException& e)
	{
		BOOST_LOG_TRIVIAL(error) << "Error querying the database: "<< std::string(e.what());
	}

	workerPool->returnWorker(worker);
	return bson;
}

mongo::BSONObj MongoDatabaseHandler::findOneByUniqueID(
	const std::string& database,
	const std::string& collection,
	const repoUUID& uuid){

	repo::core::model::bson::RepoBSON bson;
	mongo::DBClientBase *worker;
	try
	{
		repo::core::model::bson::RepoBSONBuilder queryBuilder;
		queryBuilder.append(ID, uuid);

		worker = workerPool->getWorker();
		mongo::BSONObj bsonMongo = worker->findOne(getNamespace(database, collection),
			mongo::Query(queryBuilder.obj()));

		bson = repo::core::model::bson::RepoBSON(bsonMongo);
	}
	catch (mongo::DBException& e)
	{
		BOOST_LOG_TRIVIAL(error) << e.what();
	}

	workerPool->returnWorker(worker);
	return bson;
}

std::vector<repo::core::model::bson::RepoBSON>
	MongoDatabaseHandler::getAllFromCollectionTailable(
		const std::string                             &database,
		const std::string                             &collection,
		const uint64_t                                &skip)
{
	std::vector<repo::core::model::bson::RepoBSON> bsons;
	mongo::DBClientBase *worker;
	try
	{
		worker = workerPool->getWorker();
		std::auto_ptr<mongo::DBClientCursor> cursor = worker->query(
			database + "." + collection,
			mongo::Query(),
			0,
			skip);

		while (cursor.get() && cursor->more())
		{
			//have to copy since the bson info gets cleaned up when cursor gets out of scope
			bsons.push_back(repo::core::model::bson::RepoBSON(cursor->nextSafe().copy()));
		}
	}
	catch (mongo::DBException& e)
	{
		BOOST_LOG_TRIVIAL(error) << "Failed retrieving bsons from mongo: " << e.what();
	}

	workerPool->returnWorker(worker);
	return bsons;

}

std::list<std::string> MongoDatabaseHandler::getCollections(
	const std::string &database)
{
	std::list<std::string> collections;
	mongo::DBClientBase *worker;
	try
	{
		worker = workerPool->getWorker();
		collections = worker->getCollectionNames(database);
	}
	catch (mongo::DBException& e)
	{
		BOOST_LOG_TRIVIAL(error) << e.what();
	}

	workerPool->returnWorker(worker);
	return collections;
}

repo::core::model::bson::CollectionStats MongoDatabaseHandler::getCollectionStats(
	const std::string    &database,
	const std::string    &collection,
	std::string          &errMsg)
{
	mongo::BSONObj info;
	mongo::DBClientBase *worker;
	try {
		mongo::BSONObjBuilder builder;
		builder.append("collstats", collection);
		builder.append("scale", 1); // 1024 == KB 		

		worker = workerPool->getWorker();
		worker->runCommand(database, builder.obj(), info);
	}
	catch (mongo::DBException &e)
	{
		errMsg = e.what();
		BOOST_LOG_TRIVIAL(error) << "Failed to retreive collection stats for" << database 
			<< "." << collection << " : " << errMsg;
	}

	workerPool->returnWorker(worker);
	return repo::core::model::bson::CollectionStats(info);
}

std::list<std::string> MongoDatabaseHandler::getDatabases(
	const bool &sorted)
{
	std::list<std::string> list;
	mongo::DBClientBase *worker;
	try
	{
		worker = workerPool->getWorker();
		list = worker->getDatabaseNames();

		if (sorted)
			list.sort(&MongoDatabaseHandler::caseInsensitiveStringCompare);
	}
	catch (mongo::DBException& e)
	{
		BOOST_LOG_TRIVIAL(error) << e.what();
	}
	workerPool->returnWorker(worker);
	return list;
}

std::string MongoDatabaseHandler::getProjectFromCollection(const std::string &ns, const std::string &projectExt)
{
	size_t ind = ns.find("." + projectExt);
	std::string project;
	//just to make sure string is empty.
	project.clear();
	if (ind != std::string::npos){
		project = ns.substr(0, ind);
	}
	return project;
}

std::map<std::string, std::list<std::string> > MongoDatabaseHandler::getDatabasesWithProjects(
	const std::list<std::string> &databases, const std::string &projectExt)
{
	std::map<std::string, std::list<std::string> > mapping;
	try
	{
		for (std::list<std::string>::const_iterator it = databases.begin();
			it != databases.end(); ++it)
		{
			std::string database = *it;
			std::list<std::string> projects = getProjects(database, projectExt);
			mapping.insert(std::make_pair(database, projects));
		}
	}
	catch (mongo::DBException& e)
	{
		BOOST_LOG_TRIVIAL(error) << e.what();
	}
	return mapping;
}

MongoDatabaseHandler* MongoDatabaseHandler::getHandler(
	std::string       &errMsg,
	const std::string &host, 
	const int         &port,
	const uint32_t    &maxConnections,
	const std::string &dbName,
	const std::string &username,
	const std::string &password,
	const bool        &pwDigested)
{

	std::ostringstream connectionString;

	mongo::HostAndPort hostAndPort = mongo::HostAndPort(host, port >= 0 ? port : -1);

	mongo::ConnectionString mongoConnectionString = mongo::ConnectionString(hostAndPort);


	if(!handler){
		//initialise the mongo client
		BOOST_LOG_TRIVIAL(trace) << "Handler not present for " << mongoConnectionString.toString() << " instantiating new handler...";
		try{
			handler = new MongoDatabaseHandler(mongoConnectionString, maxConnections, dbName, username, password, pwDigested);
		}
		catch (mongo::DBException e)
		{
			if (handler)
				delete handler;
			handler = 0;
			errMsg = std::string(e.what());
			BOOST_LOG_TRIVIAL(debug) << "Caught exception whilst instantiating Mongo Handler: " << errMsg;
		}
	}
	else
	{
		BOOST_LOG_TRIVIAL(trace) << "Found handler, returning existing handler";
	}


	return handler;
}

std::string MongoDatabaseHandler::getNamespace(
	const std::string &database, 
	const std::string &collection)
{	
	return database + "." + collection;
}

std::list<std::string> MongoDatabaseHandler::getProjects(const std::string &database, const std::string &projectExt)
{
	// TODO: remove db.info from the list (anything that is not a project basically)
	std::list<std::string> collections = getCollections(database);
	std::list<std::string> projects;
	for (std::list<std::string>::iterator it = collections.begin(); it != collections.end(); ++it){
		std::string project = getProjectFromCollection(*it, projectExt);
		if (!project.empty())
			projects.push_back(project);
	}
	projects.sort();
	projects.unique();
	return projects;
}

bool MongoDatabaseHandler::insertDocument(
	const std::string &database,
	const std::string &collection,
	const repo::core::model::bson::RepoBSON &obj,
	std::string &errMsg)
{
	bool success = true;
	mongo::DBClientBase *worker;
	try{
		worker = workerPool->getWorker();
		worker->insert(getNamespace(database, collection), obj);

	}
	catch (mongo::DBException &e)
	{
		success = false;
		std::string errString(e.what());
		errMsg += errString;
	}

	workerPool->returnWorker(worker);

	return success;
}

bool MongoDatabaseHandler::upsertDocument(
	const std::string &database,
	const std::string &collection,
	const repo::core::model::bson::RepoBSON &obj,
	const bool        &overwrite,
	std::string &errMsg)
{
	bool success = true;
	mongo::DBClientBase *worker;

	bool upsert = overwrite;
	try{
		worker = workerPool->getWorker();

	
		repo::core::model::bson::RepoBSONBuilder queryBuilder;
		queryBuilder << ID << obj.getField(ID);

		mongo::BSONElement bsonID;
		obj.getObjectID(bsonID);
		mongo::Query existQuery = MONGO_QUERY("_id" << bsonID);
		mongo::BSONObj bsonMongo = worker->findOne(database + "." + collection, existQuery);

		if (bsonMongo.isEmpty())
		{
			//document doens't exist, insert the document
			upsert = true;
		}

		
		if (upsert)
		{
			mongo::Query query;
			query = BSON(REPO_LABEL_ID << bsonID);
			BOOST_LOG_TRIVIAL(trace) << "query = " << query.toString();
			worker->update(getNamespace(database, collection), query, obj, true);
		}
		else
		{
			//only update fields
			mongo::BSONObjBuilder builder;
			builder << REPO_COMMAND_UPDATE << collection;

			mongo::BSONObjBuilder updateBuilder;
			updateBuilder << REPO_COMMAND_Q << BSON(REPO_LABEL_ID << bsonID);
			updateBuilder << REPO_COMMAND_U << BSON("$set" << obj.removeField(ID));
			updateBuilder << REPO_COMMAND_UPSERT << true;

			builder << REPO_COMMAND_UPDATES << BSON_ARRAY(updateBuilder.obj());
			mongo::BSONObj info;
			worker->runCommand(database, builder.obj(), info);

			if (info.hasField("writeErrors"))
			{
				BOOST_LOG_TRIVIAL(error) << info.getField("writeErrors").Array().at(0).embeddedObject().getField("errmsg");
				success = false;
			}
		}

			
	}
	catch (mongo::DBException &e)
	{
		success = false;
		std::string errString(e.what());
		errMsg += errString;
	}

	workerPool->returnWorker(worker);
	return success;
}
