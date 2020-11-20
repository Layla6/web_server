CXX ?= g++
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif
server: testMain.cc  ./timer/lst_timer.cc ./http_task/http_conn.cc ./log/log.cc ./sql_pool/sql_pool.cc  web_server.cc ./config/config.cc
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm -r server