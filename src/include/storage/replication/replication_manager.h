#pragma once

#include "loggers/storage_logger.h"

class ReplicationManager {

  // Each line in the config file should be formatted as ip:port.
  ReplicationManager(const std::string& config_path) {
    // Read from config file.
    std::ifstream replica_config(config_path);
    if (replica_config.is_open()) {
      std::getline(replica_config, replica_address);
    } else {
      STORAGE_LOG_ERROR("Replica config file is missing.");
    }
  }

private:
  std::string replica_address;
};