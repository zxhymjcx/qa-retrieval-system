from __future__ import print_function
import logging

import grpc

import qa_pb2
import qa_pb2_grpc

def run(result):
    # NOTE(gRPC Python Team): .close() is possible on a channel and should be
    # used in circumstances in which the with statement does not fit the needs
    # of the code.
    #with grpc.insecure_channel('192.168.0.105:50051') as channel:
    with grpc.insecure_channel('172.20.10.12:50051') as channel:
        stub = qa_pb2_grpc.QuaAndAnsStub(channel)
        if result:
            response = stub.GetTop5(qa_pb2.QARequest2(content="请问，可以报销抬头是个人的飞机票的电子发票吗"))
            #response = stub.GetTop5(qa_pb2.QARequest2(content="飞机票开的电子发票，抬头是个人，能报销用嘛"))
            print("Client message received: " + str(response.index))
            return
        else:
            response = stub.BuildModel(qa_pb2.QARequest(filepath="in.txt"))
        print("Client received: " + str(response.isOk))
        if response.isOk:
            response = stub.GetTop5(qa_pb2.QARequest2(content="小规模纳税人现在公司账面有盈利，股东要求分配！可以吗？怎么做分配分录"))
            print("Client message received: " + str(response.index))
if __name__ == '__main__':
    logging.basicConfig()
    run(True)
