#pragma once
#ifndef __FL_MANAGER_MANAGER_HPP
#define	__FL_MANAGER_MANAGER_HPP

///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2014 Final Level
// Author: Denys Misko <gdraal@gmail.com>
// Distributed under BSD (3-Clause) License (See
// accompanying file LICENSE)
//
// Description: Manager server control class
///////////////////////////////////////////////////////////////////////////////

#include "cluster_manager.hpp"
#include "index.hpp"
#include "cache.hpp"
#include "time_thread.hpp"

namespace fl {
	namespace metis {
		using namespace manager;
		class Manager
		{
		public:
			Manager(class Config *config);
			bool loadAll();
			IndexManager &index()
			{
				return _indexManager;
			}
			ClusterManager &clusterManager()
			{
				return _clusterManager;
			}
			Cache &cache()
			{
				return _cache;
			}
			bool fillAndAdd(ItemHeader &item, TRangePtr &range, bool &wasAdded);
			bool addLevel(const TLevel level, const TSubLevel subLevel);
			bool getPutStorages(const TRangeID rangeID, const TSize size, TStorageList &storages);
			class StorageNode *getStorageForCopy(const TRangeID rangeID, const TSize size, TStorageList &storages);
			class Config *config()
			{
				return _config;
			}
			bool startRangesChecking(EPollWorkerThread *thread);
			bool timeTic(fl::chrono::ETime &curTime);
		private:
			class Config *_config;
			ClusterManager _clusterManager;
			IndexManager _indexManager;
			Cache _cache;
			class StorageCMDRangeIndexCheck *_rangeIndexCheck;
			fl::threads::TimeThread *_timeThread;
		};
	};
};

#endif	// __FL_MANAGER_MANAGER_HPP
