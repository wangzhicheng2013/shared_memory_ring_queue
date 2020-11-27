#pragma once
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include "shared_memory_com.hpp"
namespace shmd_mdb {
const static key_t G_MDB_SHM_KEY = 0x17804;
const static size_t G_MDB_VAL_LEN = 50 * 1024 * 1024;   // 50M
class ring_mdb {
public:
    ring_mdb() = default;
    virtual ~ring_mdb() {
        if (shm_addr_ && shmdt(shm_addr_) < 0) {    // disconnect shm not to remove
            std::cerr << "shmdt failed." << std::endl;
        }
    }
public:
    bool init(key_t shm_key = G_MDB_SHM_KEY, size_t shm_size = G_MDB_VAL_LEN) {
        if (shm_size > max_mdb_val_len_) {
            std::cerr << "shm val len:" << shm_size << " over limit:" << max_mdb_val_len_ << std::endl;
            return false;
        }
        queue_len_ = shm_size;
        // remove used shm
        size_t size = shared_memory_com::get_shm_size_by_key(shm_key);
        if (size > 0 && size != queue_len_) {
            shared_memory_com::remove_shm_by_key(shm_key);
        }
        shm_addr_ = shared_memory_com::create_shm(shm_key, queue_len_);
        if (!shm_addr_) {
            return false;
        }
        ring_queue_ = static_cast<unsigned char *>(shm_addr_);
        if (!ring_queue_) {
            return false;
        }
        memset(ring_queue_, 0, queue_len_);
        last_reading_pos_ = queue_len_ - sizeof(size_t);
        return true;
    }
    inline void set_max_mdb_val_len(size_t len) {
        max_mdb_val_len_ = len;
    }
    bool put(const unsigned char *value, size_t len) {
        if (!len) {
            return false;
        }
        size_t len_size = sizeof(len);
        size_t total_len = len + len_size;
        if (total_len >= queue_len_) {
            return false;
        }
        if (currrent_writing_pos_ == last_reading_pos_) {
            return false;
        }
        if (currrent_writing_pos_ < last_reading_pos_) {
            size_t free_len = last_reading_pos_ - currrent_writing_pos_;
            if (free_len < total_len) {   // mdb is full
                return false;
            }
            memcpy(ring_queue_ + currrent_writing_pos_, &len, len_size);   // first copy len
            memcpy(ring_queue_ + currrent_writing_pos_ + len_size, value, len);    // then copy value
        }
        else  {
            if (queue_len_ - currrent_writing_pos_ >= total_len) {     // there is certain space after current writing pos
                memcpy(ring_queue_ + currrent_writing_pos_, &len, len_size);
                memcpy(ring_queue_ + currrent_writing_pos_ + len_size, value, len);
            }
            else if (last_reading_pos_ + queue_len_ - currrent_writing_pos_ >= total_len) {   // copy data to last space and first space of queue
                if (queue_len_ - currrent_writing_pos_ >= len_size) {  // there is certain space for len
                    memcpy(ring_queue_ + currrent_writing_pos_, &len, len_size);       // first copy len
                    size_t tmp_len = queue_len_ - currrent_writing_pos_ - len_size;    // copy last space of value
                    memcpy(ring_queue_ + currrent_writing_pos_ + len_size, value, tmp_len);
                    memcpy(ring_queue_, value + tmp_len, len - tmp_len);
                }
                else {
                    size_t tmp_len = queue_len_ - currrent_writing_pos_;   // copy partial space of len
                    memcpy(ring_queue_ + currrent_writing_pos_, &len, tmp_len);
                    memcpy(ring_queue_, (char *)&len + tmp_len, len_size - tmp_len);
                    memcpy(ring_queue_ + len_size - tmp_len, value, len);
                }
            }
            else {
                return false;
            }
        }
        currrent_writing_pos_ = (currrent_writing_pos_ + total_len) % queue_len_;
    }
    bool get(unsigned char *value, size_t &len) {
        if (currrent_reading_pos_ == currrent_writing_pos_) {
            return false;
        }
        get_len(currrent_reading_pos_, len);
        if (!len) {
            return false;
        }
        size_t read_pos = (currrent_reading_pos_ + sizeof(len)) % queue_len_;  // the pos for reading value
        size_t tmp_len = queue_len_ - read_pos; 
        if (len <= tmp_len) {   //  can hold the full space for value
            memcpy(value, ring_queue_ + read_pos, len);
        }
        else {
            memcpy(value, ring_queue_ + read_pos, tmp_len);  // copy the last space from read pos of queue
            memcpy(value + tmp_len, ring_queue_, len - tmp_len);    // copy from begining of queue to value
        }
        size_t tmp_pos = currrent_reading_pos_;
        currrent_reading_pos_ = (currrent_reading_pos_ + len + sizeof(len)) % queue_len_;
        last_reading_pos_ = tmp_pos;
        return true;
    }
private:
    inline void get_len(unsigned int pos, size_t &len) {
        if (pos + sizeof(len) <= queue_len_) {  // there is enough space for len
            memcpy((char *)&len, ring_queue_ + pos, sizeof(len));
            return;
        }
        memcpy((char *)&len, ring_queue_ + pos, queue_len_ - pos);  // copy partial for len
        memcpy((char *)&len + queue_len_ - pos, ring_queue_, sizeof(len) - queue_len_ + pos);   // copy the last space of len
    }
private:
    size_t max_mdb_val_len_ = 100 * 1024 * 1024;   // 100M
private:
    void *shm_addr_ = nullptr;

    unsigned char *ring_queue_ = nullptr;
    size_t queue_len_ = 0;

    unsigned int last_reading_pos_ = 0;
    unsigned int currrent_writing_pos_ = 0;
    unsigned int currrent_reading_pos_ = 0;
};
}