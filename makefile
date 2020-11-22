CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: testMain.cc ./config/config.cc  Utils.cc  ./http_task/http_conn.cc  ./timer/lst_timer.cc ./sql_pool/sql_pool.cc ./log/log.cc 
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server