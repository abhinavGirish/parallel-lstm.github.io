
#include <ostream>
#include <iostream>

#include "crossbar.hpp"
#include "bus.hpp"
#include "mig_const.hpp"
#include "mig_memory.hpp"

////////////////////////////////////////////////////////////////////////////////
// Ring
////////////////////////////////////////////////////////////////////////////////


void Ring::broadcast(CMsg msg)
{
    assert(check_directory(msg.addr));
    if(msg.type == CMsgType::busRd || msg.type == CMsgType::busRdX){
        msg.receiver = nproc;
        send(msg);
        uint64_t current_state = get_directory_info(msg.addr);
        for(size_t i=0;i<nproc;i++){
            if(current_state & 0x1 && i!=msg.sender){
                msg.receiver = i;
                send(msg);
            }
        current_state = current_state>>1;
        }
    }
    else{
        if(msg.sender==MEM)
            msg.sender=nproc;
        send(msg);
    }
}

void Ring::send(CMsg msg){
    num_messages++;
    interconnects[msg.sender].incomming.push(msg);
}

void Ring::send_ack(CMsg msg)
{
    if(msg.flags & 0x2)
    {
        migratory++;
    }
    assert(check_directory(msg.addr));
    broadcast(msg);
    if(msg.flags & MigBusFlag::TRANSMIT){
        msg.receiver = nproc;
        broadcast(msg);
    }
}

void Ring::event()
{
  for(size_t i=0;i<nproc+1;i++){
    if(interconnects[i].delay.is_done())
    {
            if(interconnects[i].incomming.size() > 0)
            {
                interconnects[i].delay.reset(BUS_DLY);
            }
    }
    else
    {
        if(interconnects[i].delay.tick())
        {
            CMsg msg = interconnects[i].incomming.front();
            assert(check_directory(msg.addr));
            interconnects[i].incomming.pop();
            int index = i;
            int left_index;
            int right_index;
            if(index-1<0){
                left_index = nproc;
            }
            else{
                left_index = index -1;
            }
            if(index+1>nproc){
                right_index = 0;
            }
            else{
                right_index = index + 1;
            }
            if(left_index == msg.receiver || right_index == msg.receiver){
                assert(msg.receiver <= nproc);
                receivers[msg.receiver]->receive(msg);
            }
            else{
                int right_distance;
                int left_distance;
                if(index < msg.receiver){
                    right_distance = msg.receiver - index;
                    left_distance = index + (nproc+1 - msg.receiver);
                }
                else{
                    left_distance = index - msg.receiver;
                    right_distance = msg.receiver + (nproc+1 - index);
                }
                if(left_distance < right_distance){
                    interconnects[left_index].incomming.push(msg);
                }
                else if(left_distance > right_distance){
                    interconnects[right_index].incomming.push(msg);
                }
                else{
                    if(interconnects[left_index].incomming.size()<interconnects[right_index].incomming.size()){
                        interconnects[left_index].incomming.push(msg);
                    }
                    else{
                        interconnects[right_index].incomming.push(msg);
                    }
                }
            }
        }
    }
  }
}


bool Ring::check_directory(uint64_t addr){
    uint64_t current_state = get_directory_info(addr);
    for(size_t i=0;i<nproc;i++){
        Line *line;
        bool found = cache_refs[i]->find_line(addr,&line);
        if(found && !((current_state>>i) & 0x1))
            return false;
        //if(!found && ((current_state>> i) & 0x1))
        //    return false;
    }
    return true;
}

uint32_t Ring::num_proc(uint64_t addr){
    uint64_t num = get_directory_info(addr);
    uint32_t ans = 0;
    while(num != 0){
        if(num & 0x1)
            ans++;
        num = num >> 1;
    }
    return ans;
}