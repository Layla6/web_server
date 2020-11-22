#include "lst_timer.h"
//ascend
sortimer_lst::sortimer_lst(){
    head=NULL;
    tail=NULL;
}
sortimer_lst::~sortimer_lst(){
    timer_node* tmp=head;
    while(tmp){
        head=tmp->next;
        delete tmp;
        tmp=head;
    }
}
void sortimer_lst::add_timer(timer_node* item){
    if(item==NULL)
        return;
    if(head==NULL){
        head=tail=item;
        return ;
    }
    if(item->expire<head->expire){
        item->next=head;
        head->prev=item;
        head=item;
        return;
    }
    add_timer(item,head);
}

void sortimer_lst::adjust_timer(timer_node* item){
    if (!item){
        return;
    }
    timer_node *tmp = item->next;
    if (!tmp || (item->expire < tmp->expire)){
        return;
    }
    if (item == head){
        head = head->next;
        head->prev = NULL;
        item->next = NULL;
        add_timer(item, head);
    }
    else{
        item->prev->next = item->next;
        item->next->prev = item->prev;
        add_timer(item, item->next);
    }
}

void sortimer_lst::del_timer(timer_node* item){
    if (!item)
        return;
    if ((item == head) && (item == tail)){
        delete item;
        head = NULL;
        tail = NULL;
        return;
    }
    if (item == head){
        head = head->next;
        head->prev = NULL;
        delete item;
        return;
    }
    if (item == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete item;
        return;
    }
    item->prev->next = item->next;
    item->next->prev = item->prev;
    delete item;
}
void sortimer_lst::add_timer(timer_node *timer, timer_node *lst_head){
    timer_node *prev = lst_head;
    timer_node *tmp = prev->next;
    while (tmp){
        if (timer->expire < tmp->expire){
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}
void sortimer_lst::tick(){
    if(head==NULL)
        return ;
    time_t cur=time(NULL);
    timer_node* tmp=head;
    while(tmp){
        //没有超时
        if(cur<tmp->expire)
            break;
        tmp->cb_func(tmp->user_data);
        head=tmp->next;
        if(head)
            head->prev=NULL;
        delete tmp;
        tmp=head;
    }

}