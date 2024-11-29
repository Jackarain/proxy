FROM ubuntu:22.04 as builder

RUN apt-get update && apt-get install --fix-missing -y ca-certificates
RUN sed -i "s@http://.*archive.ubuntu.com@https://mirrors.pku.edu.cn@g" /etc/apt/sources.list && sed -i "s@http://.*security.ubuntu.com@https://mirrors.pku.edu.cn@g" /etc/apt/sources.list
RUN apt-get update && apt-get upgrade -y
RUN apt-get install -y cmake gcc g++ ninja-build
ADD . /proxy
RUN cd /proxy && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja && ninja simple_proxy data_flow
RUN cd /proxy/test/lnx && g++ -std=c++20 test_dataflow.cpp -o test_dataflow -ldl -pthread && g++ -std=c++20 test_dataflow_ws.cpp -o test_dataflow_ws -ldl -pthread && g++ -std=c++20 test_dataflow_ws_manager.cpp -o test_dataflow_ws_manager -ldl -pthread && g++ -std=c++20 test_dataflow_ws_params.cpp -o test_dataflow_ws_params -ldl -pthread
RUN cd /root && mkdir -p lib && cp /proxy/build/lib/libdata_flow.so /root/lib/ && cp /proxy/build/third_party/boost/libs/url/libboost_url.so.1.86.0 /root/lib/ && cp /proxy/test/lnx/test_dataflow /proxy/test/lnx/test_dataflow_ws /proxy/test/lnx/test_dataflow_ws_params /proxy/test/lnx/test_dataflow_ws_manager /root/ && cp /proxy/test/flow.conf /root/
# RUN cd /proxy && mkdir -p wolfssl && cd wolfssl && cmake .. -DENABLE_USE_OPENSSL=OFF -DENABLE_USE_WOLFSSL=ON -DCMAKE_BUILD_TYPE=Release -G Ninja && ninja

FROM ubuntu:22.04
ENV LD_LIBRARY_PATH=/root/lib
RUN apt-get update && apt-get install -y ca-certificates
RUN cd /root && mkdir -p lib
COPY --from=builder /root/lib/libdata_flow.so /root/lib/libdata_flow.so
COPY --from=builder /root/lib/libboost_url.so.1.86.0 /root/lib/libboost_url.so.1.86.0
RUN 
COPY --from=builder /root/test_dataflow /root/test_dataflow
COPY --from=builder /root/test_dataflow_ws /root/test_dataflow_ws
COPY --from=builder /root/test_dataflow_ws_params /root/test_dataflow_ws_params
COPY --from=builder /root/test_dataflow_ws_manager /root/test_dataflow_ws_manager
COPY --from=builder /root/flow.conf /root/flow.conf
WORKDIR /root/
# COPY --from=builder /proxy/wolfssl/bin/proxy_server /usr/local/bin/proxy_server-wolfssl
