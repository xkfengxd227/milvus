// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "milvus.grpc.pb.h"
#include "GrpcMilvusServer.h"
#include "../ServerConfig.h"
#include "../DBWrapper.h"
#include "utils/Log.h"
#include "faiss/utils.h"
#include "GrpcRequestHandler.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/grpcpp.h>


namespace zilliz {
namespace milvus {
namespace server {
namespace grpc {

static std::unique_ptr<::grpc::Server> server;

constexpr long MESSAGE_SIZE = -1;

class NoReusePortOption : public ::grpc::ServerBuilderOption {
 public:
    void UpdateArguments(::grpc::ChannelArguments *args) override {
        args->SetInt(GRPC_ARG_ALLOW_REUSEPORT, 0);
    }

    void UpdatePlugins(std::vector<std::unique_ptr<::grpc::ServerBuilderPlugin>> *
    plugins) override {}
};


Status
GrpcMilvusServer::StartService() {
    if (server != nullptr) {
        std::cout << "stop service!\n";
        StopService();
    }

    ServerConfig &config = ServerConfig::GetInstance();
    ConfigNode server_config = config.GetConfig(CONFIG_SERVER);
    ConfigNode engine_config = config.GetConfig(CONFIG_ENGINE);
    std::string address = server_config.GetValue(CONFIG_SERVER_ADDRESS, "127.0.0.1");
    int32_t port = server_config.GetInt32Value(CONFIG_SERVER_PORT, 19530);

    faiss::distance_compute_blas_threshold = engine_config.GetInt32Value(CONFIG_DCBT, 20);

    std::string server_address(address + ":" + std::to_string(port));

    ::grpc::ServerBuilder builder;
    builder.SetOption(std::unique_ptr<::grpc::ServerBuilderOption>(new NoReusePortOption));
    builder.SetMaxReceiveMessageSize(MESSAGE_SIZE); //default 4 * 1024 * 1024
    builder.SetMaxSendMessageSize(MESSAGE_SIZE);

    builder.SetCompressionAlgorithmSupportStatus(GRPC_COMPRESS_STREAM_GZIP, true);
    builder.SetDefaultCompressionAlgorithm(GRPC_COMPRESS_STREAM_GZIP);
    builder.SetDefaultCompressionLevel(GRPC_COMPRESS_LEVEL_HIGH);

    GrpcRequestHandler service;

    builder.AddListeningPort(server_address, ::grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    server = builder.BuildAndStart();
    server->Wait();

    return Status::OK();
}

Status
GrpcMilvusServer::StopService() {
    if (server != nullptr) {
        server->Shutdown();
    }

    return Status::OK();
}

}
}
}
}