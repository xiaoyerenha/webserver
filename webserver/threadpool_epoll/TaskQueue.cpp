#include "./include/TaskQueue.h"

template <typename T>
TaskQueue<T>::TaskQueue() {
	//互斥锁
	pthread_mutex_init(&m_mutex, NULL);
}

template <typename T>
TaskQueue<T>::~TaskQueue() {
	pthread_mutex_destroy(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(http_conn<T> task) {
	pthread_mutex_lock(&m_mutex);
	m_taskQ.push(task);
	pthread_mutex_unlock(&m_mutex);
}
/*
template <typename T>
void TaskQueue<T>::addTask(callback f, void* arg) {
	pthread_mutex_lock(&m_mutex);
	m_taskQ.push(http_conn(f, arg));
	pthread_mutex_unlock(&m_mutex);
}
*/
template <typename T>
http_conn<T> TaskQueue<T>::takeTask() {
	http_conn<T> t;
	pthread_mutex_lock(&m_mutex);
	if(!m_taskQ.empty()) {
		t = m_taskQ.front();
		m_taskQ.pop();
	}
	pthread_mutex_unlock(&m_mutex);
	return t;
}
