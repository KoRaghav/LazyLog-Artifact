#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <iomanip>
#include <deque>

#include "lazylog_cli.h"

#include "../utils/properties.h"

using namespace lazylog;

enum EventType { INVOKE = 0, RESPONSE = 1 };
enum OpType { OP_WRITE = 0, OP_READ = 1 };

struct HistoryRecord {
    uint64_t req_id;
    int thread_id;
    EventType event_type;
    OpType op_type;
    std::string value;
    int64_t idx;
    std::vector<uint64_t> seq_nums = {};
};
static std::atomic<uint64_t> global_req_id{0};

static std::vector<HistoryRecord> global_history;
static std::mutex history_mtx;
std::deque<uint64_t> writes;
static int64_t max_write_idx = -1;

void RecordWriteInvoke(uint64_t req, int thread_id, const std::string& payload) {
    std::lock_guard<std::mutex> lock(history_mtx);
    global_history.push_back({req, thread_id, INVOKE, OP_WRITE, payload, -1});
    max_write_idx++;
}

void RecordWriteResponse(uint64_t req, int thread_id, int64_t idx, std::vector<uint64_t> seq_nums) {
    std::lock_guard<std::mutex> lock(history_mtx);
    global_history.push_back({req, thread_id, RESPONSE, OP_WRITE, "", idx, seq_nums});
}

uint64_t RecordReadInvoke(uint64_t req, int thread_id) {
    std::lock_guard<std::mutex> lock(history_mtx);
    global_history.push_back({req, thread_id, INVOKE, OP_READ, "", -1});
    return max_write_idx;
}

void RecordReadResponse(uint64_t req, int thread_id, const std::string& data, int64_t idx) {
    std::lock_guard<std::mutex> lock(history_mtx);
    global_history.push_back({req, thread_id, RESPONSE, OP_READ, data, idx});
}

void DumpGlobalHistory(const std::string& filename) {
    std::lock_guard<std::mutex> lock(history_mtx);
    std::ofstream f(filename);

    if (!f.is_open()) {
        std::cerr << "Failed to open output file\n";
        return;
    }
    
    for (auto& rec : global_history) {
        if (rec.event_type == INVOKE && rec.op_type == OP_WRITE) {
            f << "inv write id=" << rec.req_id
              << " client=" << rec.thread_id
              << " val=" << rec.value << "\n";
        } 
        else if (rec.event_type == RESPONSE && rec.op_type == OP_WRITE) {
            f << "res write id=" << rec.req_id
              << " client=" << rec.thread_id
              << " idx=" << rec.idx
              << " seq=[";
            for (int i=0; i< rec.seq_nums.size(); i++) {
                f << rec.seq_nums[i];
                if (i < rec.seq_nums.size() - 1) f << ',';
            }
            f << ']' << '\n';
        }
        else if (rec.event_type == INVOKE && rec.op_type == OP_READ) {
            f << "inv read id=" << rec.req_id
              << " client=" << rec.thread_id << "\n";
        }
        else {
            f << "res read id=" << rec.req_id
              << " client=" << rec.thread_id
              << " idx=" << rec.idx
              << " val=" << rec.value << "\n";
        }
    }

    f.close();
}

void WriterThread(int thread_id, int num_ops, Properties prop) {
    std::string uid = std::to_string(thread_id);
    prop.SetProperty("dur_log.client_id", uid);

    LazyLogClient cli(uid);
    cli.Initialize(prop);


    for (int i = 0; i < num_ops; i++) {
        uint64_t req = global_req_id.fetch_add(1);
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << thread_id << '_';
        oss << std::setw(5) << std::setfill('0') << i;
        std::string payload = oss.str();
    
        RecordWriteInvoke(req, thread_id, payload);
        std::vector<uint64_t> seq_nums = cli.AppendEntry(payload);
        RecordWriteResponse(req, thread_id, 0, seq_nums);
    }
}

void ReaderThread(int thread_id, int num_ops, Properties prop) {
    std::string uid = std::to_string(thread_id);
    prop.SetProperty("dur_log.client_id", uid);

    LazyLogClient cli(uid);
    cli.Initialize(prop);

    std::string data;

    for (int i = 0; i < num_ops; i++) {
        uint64_t req = global_req_id.fetch_add(1);
    
        RecordReadInvoke(req, thread_id);
    
        cli.ReadEntry(i, data);
    
        RecordReadResponse(req, thread_id, data, i);
    }
}

int main(int argc, const char *argv[]) {
    Properties prop;
    ParseCommandLine(argc, argv, prop);

    int ops_per_thread = prop.ContainsKey("count") ? std::stoi(prop.GetProperty("count")) : 10;
    int total_threads = prop.ContainsKey("threads") ? std::stoi(prop.GetProperty("threads")) : 5;
    float write_ratio = prop.ContainsKey("ratio") ? std::stod(prop.GetProperty("ratio")) : 0.8;

    int num_writers = static_cast<int>(total_threads * write_ratio);
    if (write_ratio > 0 && num_writers == 0 && total_threads > 0) num_writers = 1;
    int num_readers = total_threads - num_writers;

    std::cout << "Running with " << num_writers << " writers and " << num_readers << " readers.\n";

    std::vector<std::thread> threads;
    threads.reserve(total_threads);

    for (int i = 0; i < num_writers; i++) {
        threads.emplace_back(WriterThread, i, ops_per_thread, prop);
    }

    for (int i = 0; i < num_readers; i++) {
        threads.emplace_back(ReaderThread, num_writers + i, ops_per_thread * num_writers, prop);
    }

    for (auto &t : threads) {
        if (t.joinable()) t.join();
    }

    DumpGlobalHistory("execution_history.log");
    return 0;
}