#pragma once
#include <utility>
#include "common/macros.h"
#include "common/managed_pointer.h"
#include "network/connection_context.h"
#include "network/network_defs.h"
#include "network/network_types.h"
#include "network/postgres/postgres_protocol_utils.h"
#include "network/protocol_interpreter.h"

#define DEFINE_COMMAND(name, flush)                                                                           \
  class name : public AbstractNetworkCommand {                                                                \
   public:                                                                                                    \
    explicit name(InputPacket *in) : AbstractNetworkCommand(in, flush) {}                                     \
    Transition Exec(common::ManagedPointer<ProtocolInterpreter> interpreter,                                  \
                    common::ManagedPointer<PostgresPacketWriter> out,                                         \
                    common::ManagedPointer<trafficcop::TrafficCop> t_cop,                                     \
                    common::ManagedPointer<ConnectionContext> connection, NetworkCallback callback) override; \
  }

namespace terrier::network {

/**
 * Interface for the execution of the standard PostgresNetworkCommands for the postgres protocol
 */
class AbstractNetworkCommand {
 public:
  /**
   * Executes the command
   * @param interpreter The protocol interpreter that called this
   * @param out The Writer on which to construct output packets for the client
   * @param t_cop The traffic cop pointer
   * @param connection The ConnectionContext which contains connection information
   * @param callback The callback function to trigger after
   * @return The next transition for the client's state machine
   */
  virtual Transition Exec(common::ManagedPointer<ProtocolInterpreter> interpreter,
                          common::ManagedPointer<PostgresPacketWriter> out,
                          common::ManagedPointer<trafficcop::TrafficCop> t_cop,
                          common::ManagedPointer<ConnectionContext> connection, NetworkCallback callback) = 0;

  /**
   * @return Whether or not to flush the output network packets from this on completion
   */
  bool FlushOnComplete() { return flush_on_complete_; }

  /**
   * Default destructor
   */
  virtual ~AbstractNetworkCommand() = default;

 protected:
  /**
   * Constructor for a NetworkCommand instance
   * @param in The input packets to this command
   * @param flush Whether or not to flush the outuput packets on completion
   */
  AbstractNetworkCommand(InputPacket *in, bool flush)
      : in_(in->buf_->ReadIntoView(in->len_)), flush_on_complete_(flush) {}

  /**
   * The ReadBufferView to read input packets from
   */
  ReadBufferView in_;

 private:
  bool flush_on_complete_;
};
}  // namespace terrier::network
