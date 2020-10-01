#pragma once

#include <fstream>
#include <common/container/concurrent_queue.h>
#include "common/json.h"
#include "loggers/storage_logger.h"
#include "storage/write_ahead_log/log_io.h"
#include "storage/write_ahead_log/log_record.h"
#include "messenger/messenger.h"

namespace terrier::storage {

class ReplicationManager {
public:
  // Each line in the config file should be formatted as ip:port.
  ReplicationManager(common::ManagedPointer<messenger::MessengerLogic> messenger_logic, const std::string& config_path) : messenger_(messenger_logic) {
    // Read from config file.
    std::ifstream replica_config(config_path);
    if (replica_config.is_open()) {
      std::getline(replica_config, replica_address);
    } else {
      STORAGE_LOG_ERROR("Replica config file is missing.");
    }
  }

  void AddRecordBuffer(BufferedLogWriter *network_buffer) {
    replication_consumer_queue_->Enqueue(std::make_pair(network_buffer, std::vector<CommitCallback>()));
  }

  nlohmann::json Serialize() {
    // Grab buffers in queue
    std::deque<BufferedLogWriter *> temp_buffer_queue;
    uint64_t data_size = 0;
    SerializedLogs logs;
    while (!replication_consumer_queue_->Empty()) {
      replication_consumer_queue_->Dequeue(&logs);
      data_size += logs.first->GetBufferSize();
      temp_buffer_queue.push_back(logs.first);
    }
    TERRIER_ASSERT(data_size > 0, "Amount of data to send must be greater than 0");

    // Build JSON object.
    nlohmann::json j;
    j["type"] = "itp";

    std::string content;
    uint64_t size;
    // Change to vector
    for (auto *buffer : temp_buffer_queue) {
      size += buffer->GetBufferSize();
      content += buffer->GetBuffer();
    }

    j["size"] = size;
    j["content"] = content;

    return j;
  }

private:
  std::string replica_address;
  common::ConcurrentQueue<SerializedLogs> *replication_consumer_queue_;
  messenger::Messenger messenger_;
};

}  // namespace terrier::storage;