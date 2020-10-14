#include<iostream>
#include<unistd.h>
#include<pthread.h>
using namespace std;
pthread_mutex_t mtx1=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx2=PTHREAD_MUTEX_INITIALIZER;
void *myfunction1(void *arg){
	for(int i=0;i<10;++i){
		pthread_mutex_lock(&mtx1);
		cout<<"this is  the first thread!"<<endl;
		pthread_mutex_unlock(&mtx1);
		//sleep(1);
	}
}
void *myfunction2(void *arg){
	for(int i=0;i<10;++i){
		pthread_mutex_lock(&mtx1);
		cout<<"this is  the second thread!"<<endl;
		pthread_mutex_unlock(&mtx1);
		//sleep(1);
	}
}

int main(){
	int ret=0;
	pthread_t id1,id2;
	//ret=pthread_create(&id1,NULL,(void*)myfunction1,NULL);
	
	ret=pthread_create(&id1,NULL,myfunction1,NULL);
	if(ret){
		cout<<"error:create pthread!"<<endl;
		return 1;
	}
	
	//ret=pthread_create(&id2,NULL,(void*)myfunction2,NULL);
	ret=pthread_create(&id2,NULL,myfunction2,NULL);
	if(ret){
		cout<<"error:create pthread!"<<endl;
		return 1;
	}
	pthread_join(id1,NULL);
	
	pthread_join(id2,NULL);
	return 0;
}
