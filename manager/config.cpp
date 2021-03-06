///////////////////////////////////////////////////////////////////////////////
//
// Copyright Denys Misko <gdraal@gmail.com>, Final Level, 2014.
// Distributed under BSD (3-Clause) License (See
// accompanying file LICENSE)
//
// Description: Metis manager's server configuration class implementation
///////////////////////////////////////////////////////////////////////////////

#include <boost/property_tree/ini_parser.hpp>

#include "config.hpp"
#include "log.hpp"
#include "metis_log.hpp"
#include "util.hpp"

using namespace fl::metis;
using namespace boost::property_tree::ini_parser;
using fl::db::ESC;

Config::Config(int argc, char *argv[])
	: GlobalConfig(argc, argv), _serverID(0), _status(0), _logLevel(FL_LOG_LEVEL), _cmdPort(0), _webDavPort(0), 
	_webPort(0), _cmdTimeout(0), _webTimeout(0), _webDavTimeout(0), _webWorkerQueueLength(0), _webWorkers(0),	
	_cmdWorkerQueueLength(0), _cmdWorkers(0), _bufferSize(0), _maxFreeBuffers(0), _minimumCopies(0), 
	_maxConnectionPerStorage(0), _averageItemSize(0), _cacheSize(0), _itemHeadersCacheSize(0), _itemsInLine(0),
	_minHitsToCache(0)
{
	char ch;
	optind = 1;
	while ((ch = getopt(argc, argv, "s:c:")) != -1) {
		switch (ch) {
			case 's':
				_serverID = atoi(optarg);
			break;
		}
	}
	if (!_serverID) {
		printf("serverID is required\n");
		_usage();
		throw std::exception();
	}

	try
	{
		_logPath = _pt.get<decltype(_logPath)>("metis-manager.log", _logPath);
		_logLevel = _pt.get<decltype(_logLevel)>("metis-manager.logLevel", _logLevel);
		if (_pt.get<std::string>("metis-manager.logStdout", "on") == "on")
			_status |= ST_LOG_STDOUT;

		_parseUserGroupParams(_pt, "metis-manager");
		_loadFromDB();
		_loadCacheParams();
		
		_cmdTimeout =  _pt.get<decltype(_webTimeout)>("metis-manager.cmdTimeout", DEFAULT_SOCKET_TIMEOUT);		
		_webTimeout =  _pt.get<decltype(_webTimeout)>("metis-manager.webTimeout", DEFAULT_SOCKET_TIMEOUT);
		_webDavTimeout =  _pt.get<decltype(_webTimeout)>("metis-manager.webDavTimeout", DEFAULT_SOCKET_TIMEOUT);

		
		_webWorkerQueueLength = _pt.get<decltype(_webWorkerQueueLength)>("metis-manager.webSocketQueueLength", 
			DEFAULT_SOCKET_QUEUE_LENGTH);
		_webWorkers = _pt.get<decltype(_webWorkers)>("metis-manager.webWorkers", DEFAULT_WORKERS_COUNT);
		
		_cmdWorkerQueueLength = _pt.get<decltype(_cmdWorkerQueueLength)>("metis-manager.cmdSocketQueueLength", 
			DEFAULT_SOCKET_QUEUE_LENGTH);
		_cmdWorkers = _pt.get<decltype(_cmdWorkers)>("metis-manager.cmdWorkers", DEFAULT_WORKERS_COUNT);

		_bufferSize = _pt.get<decltype(_bufferSize)>("metis-manager.bufferSize", DEFAULT_BUFFER_SIZE);
		_maxFreeBuffers = _pt.get<decltype(_maxFreeBuffers)>("metis-manager.maxFreeBuffers", DEFAULT_MAX_FREE_BUFFERS);
		
		_minimumCopies = _pt.get<decltype(_minimumCopies)>("metis-manager.minimumCopies", DEFAULT_MINIMUM_COPIES);
		
		_maxConnectionPerStorage = _pt.get<decltype(_maxConnectionPerStorage)>("metis-manager.maxConnectionPerStorage", 
			DEFAULT_MAX_CONNECTION_PER_STORAGE);
		
		_averageItemSize = _pt.get<decltype(_averageItemSize)>("metis-manager.averageItemSize", 
			DEFAULT_AVERAGE_ITEM_SIZE);
	}
	catch (ini_parser_error &err)
	{
		printf("Caught error %s when parse %s at line %lu\n", err.message().c_str(), err.filename().c_str(), err.line());
		throw err;
	}
	catch(...)
	{
		printf("Caught unknown exception while parsing ini file %s\n", _configFileName.c_str());
		throw;
	}
}

void Config::_usage()
{
	printf("usage: metis_manager -s serverID [-c configPath]\n");
}

void Config::_loadCacheParams()
{
	_cacheSize = fl::utils::parseSizeString(_pt.get<std::string>("metis-manager.cacheSize", "0").c_str());
	_itemHeadersCacheSize = fl::utils::parseSizeString(
		_pt.get<std::string>("metis-manager.itemHeadersCacheSize", "0").c_str());
	if (!_itemHeadersCacheSize) {
		_itemHeadersCacheSize = _cacheSize * 0.1;
		_cacheSize -= _itemHeadersCacheSize;
	}
	_itemsInLine = _pt.get<decltype(_itemsInLine)>("metis-manager.itemsInLine", DEFAULT_ITEMS_IN_LINE);
	_minHitsToCache = _pt.get<decltype(_minHitsToCache)>("metis-manager.minHitsToCache", 1);
}

void Config::_loadFromDB()
{
	Mysql sql;
	if (!connectDb(sql)) {
		printf("Cannot connect to db, check db parameters\n");
		throw std::exception();
	}
	enum EManagerFlds
	{
		FLD_CMD_IP,
		FLD_CMD_PORT,
		FLD_WEB_DAV_IP,
		FLD_WEB_DAV_PORT,
		FLD_WEB_IP,
		FLD_WEB_PORT,
	};
	auto res = sql.query(sql.createQuery() << "SELECT cmdIp, cmdPort, webDavIp, webDavPort, webIp, webPort \
		FROM manager WHERE id=" << ESC << _serverID);
	if (!res || !res->next())	{
		printf("Cannot get information about %u manager\n", _serverID);
		throw std::exception();
	}
	_cmdIp = res->get<decltype(_cmdIp)>(FLD_CMD_IP);
	_cmdPort = res->get<decltype(_cmdPort)>(FLD_CMD_PORT);
	
	_webDavIp = res->get<decltype(_webDavIp)>(FLD_WEB_DAV_IP);
	_webDavPort = res->get<decltype(_webDavPort)>(FLD_WEB_DAV_PORT);

	_webIp = res->get<decltype(_webIp)>(FLD_WEB_IP);
	_webPort = res->get<decltype(_webPort)>(FLD_WEB_PORT);

}

bool Config::initNetwork()
{
	if (!_cmdSocket.listen(_cmdIp.c_str(), _cmdPort))	{
		log::Error::L("Metis manager %u command interface cannot begin listening on %s:%u\n", 
			_serverID, _cmdIp.c_str(), _cmdPort);
		return false;
	}
	log::Warning::L("Metis manager %u command interface is listening on %s:%u\n", _serverID, _cmdIp.c_str(), _cmdPort);

	if (!_webDavSocket.listen(_webDavIp.c_str(), _webDavPort))	{
		log::Error::L("Metis manager %u WebDAV interface cannot begin listening on %s:%u\n", 
			_serverID, _webDavIp.c_str(), _webDavPort);
		return false;
	}
	log::Warning::L("Metis manager %u WebDAV interface is listening on %s:%u\n", 
		_serverID, _webDavIp.c_str(), _webDavPort);

	if (!_webSocket.listen(_webIp.c_str(), _webPort))	{
		log::Error::L("Metis manager %u web interface cannot begin listening on %s:%u\n", 
			_serverID, _webIp.c_str(), _webPort);
		return false;
	}
	log::Warning::L("Metis manager %u web interface is listening on %s:%u\n", 
		_serverID, _webIp.c_str(), _webPort);

	return true;
}



