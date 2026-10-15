#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <omp.h>

enum class Op { Add, Sub, Mul, Div, Mod };
struct Job { Op op; int a, b; };
struct Result { int value; std::uint64_t steps; };

// A job owns one tape and one head. Every read/write moves through these
// primitives; no operation substitutes a host-language arithmetic answer.
class Machine {
    std::string tape_;
    std::size_t head_ = 0;
    std::uint64_t steps_ = 0;
    std::string state_ = "start";
public:
    Machine(int a, int b) : tape_(std::string(a, '1') + "#" + std::string(b, '1') + "|"), head_(0) {}
    char read() { ++steps_; return tape_.at(head_); }
    void write(char c) { ++steps_; tape_.at(head_) = c; }
    void right() { ++steps_; if (++head_ == tape_.size()) tape_.push_back('_'); }
    void left() { ++steps_; if (head_ == 0) throw std::logic_error("head underflow"); --head_; }
    void rewind() { state_ = "rewind"; while (head_) left(); }
    void seek(char c) { state_ = "seek"; while (read() != c) right(); }
    // Locate the first unmarked symbol in a region bounded by a delimiter.
    bool take(char begin, char end, char mark) {
        rewind(); seek(begin); right(); state_ = "scan";
        while (read() != end) {
            if (read() == '1') { write(mark); return true; }
            right();
        }
        return false;
    }
    // Input A starts at index zero, so its synthetic left delimiter is omitted.
    bool take_a(char mark) {
        rewind(); state_ = "scan_a";
        while (read() != '#') {
            if (read() == '1') { write(mark); return true; }
            right();
        }
        return false;
    }
    bool take_b(char mark) { return take('#', '|', mark); }
    void restore(char from) {
        rewind(); state_ = "restore";
        while (read() != '|') { if (read() == from) write('1'); right(); }
    }
    void append() {
        rewind(); seek('|'); right(); state_ = "append";
        while (read() == '1') right();
        write('1'); right();
    }
    int output() {
        rewind(); seek('|'); right(); state_ = "count";
        int count = 0;
        while (read() == '1') { ++count; right(); }
        state_ = "halt";
        return count;
    }
    std::uint64_t steps() const { return steps_; }
    const std::string& tape() const { return tape_; }
};

Result run(const Job& job, std::string* trace = nullptr) {
    if (job.a < 0 || job.b < 0 || job.a > 1000 || job.b > 1000)
        throw std::invalid_argument("operands must be in 0..1000");
    if ((job.op == Op::Div || job.op == Op::Mod) && job.b == 0)
        throw std::invalid_argument("division by zero");
    Machine m(job.a, job.b);
    switch (job.op) {
    case Op::Add:
        while (m.take_a('x')) m.append();
        while (m.take_b('x')) m.append();
        break;
    case Op::Sub:
        while (m.take_b('y')) {
            if (!m.take_a('x')) break;
        }
        while (m.take_a('x')) m.append();
        break;
    case Op::Mul:
        while (m.take_a('x')) {
            while (m.take_b('y')) m.append();
            m.restore('y');
        }
        break;
    case Op::Div:
    case Op::Mod: {
        // Trial subtraction marks B and A. Restore A on an incomplete trial.
        while (true) {
            bool complete = true;
            while (m.take_b('y')) {
                if (!m.take_a('t')) { complete = false; break; }
            }
            m.restore('y');
            if (!complete) { m.restore('t'); break; }
            // Commit trial: t becomes consumed x, one symbol at a time.
            m.rewind();
            while (m.read() != '#') { if (m.read() == 't') m.write('x'); m.right(); }
            if (job.op == Op::Div) m.append();
        }
        if (job.op == Op::Mod) while (m.take_a('x')) m.append();
        break;
    }
    }
    int value = m.output();
    if (trace) *trace = m.tape();
    return {value, m.steps()};
}

int expected(const Job& j) {
    switch (j.op) {
    case Op::Add: return j.a + j.b;
    case Op::Sub: return std::max(0, j.a - j.b);
    case Op::Mul: return j.a * j.b;
    case Op::Div: return j.a / j.b;
    case Op::Mod: return j.a % j.b;
    }
    throw std::logic_error("unknown operation");
}
std::string name(Op op) {
    switch (op) {
    case Op::Add: return "add"; case Op::Sub: return "sub";
    case Op::Mul: return "mul"; case Op::Div: return "div"; case Op::Mod: return "mod";
    }
    return "?";
}
Op parse_op(const std::string& s) {
    for (int i = 0; i < 5; ++i) if (name(static_cast<Op>(i)) == s) return static_cast<Op>(i);
    throw std::invalid_argument("operation must be add|sub|mul|div|mod");
}
std::vector<Job> generate(std::size_t n, int max_operand, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> operand(0, max_operand), operation(0, 4);
    std::vector<Job> jobs; jobs.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        Op op = static_cast<Op>(operation(rng));
        int a = operand(rng), b = operand(rng);
        if ((op == Op::Div || op == Op::Mod) && b == 0) b = 1;
        jobs.push_back({op, a, b});
    }
    return jobs;
}
void execute(const std::vector<Job>& jobs, std::vector<Result>& out, int threads, bool dynamic) {
    if (threads == 0) {
        for (std::size_t i = 0; i < jobs.size(); ++i) out[i] = run(jobs[i]);
    } else if (dynamic) {
        #pragma omp parallel for schedule(dynamic, 16) num_threads(threads)
        for (std::int64_t i = 0; i < static_cast<std::int64_t>(jobs.size()); ++i) out[i] = run(jobs[i]);
    } else {
        #pragma omp parallel for schedule(static) num_threads(threads)
        for (std::int64_t i = 0; i < static_cast<std::int64_t>(jobs.size()); ++i) out[i] = run(jobs[i]);
    }
}
void check(const std::vector<Job>& jobs, const std::vector<Result>& out) {
    for (std::size_t i = 0; i < jobs.size(); ++i)
        if (out[i].value != expected(jobs[i]))
            throw std::runtime_error("mismatch at job " + std::to_string(i) + " " + name(jobs[i].op) +
                "(" + std::to_string(jobs[i].a) + "," + std::to_string(jobs[i].b) + "): " + std::to_string(out[i].value));
}
int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::invalid_argument("usage: unary test | show OP A B | bench JOBS MAX_OPERAND REPEATS SEED");
        std::string mode = argv[1];
        if (mode == "show") {
            if (argc != 5) throw std::invalid_argument("show OP A B");
            Job j{parse_op(argv[2]), std::stoi(argv[3]), std::stoi(argv[4])};
            std::string tape; Result r = run(j, &tape);
            std::cout << "result=" << r.value << " steps=" << r.steps << " tape=" << tape << '\n';
        } else if (mode == "test") {
            std::vector<Job> jobs;
            for (int op = 0; op < 5; ++op)
                for (int a = 0; a <= 20; ++a)
                    for (int b = 0; b <= 20; ++b)
                        if (b || (op != 3 && op != 4)) jobs.push_back({static_cast<Op>(op), a, b});
            auto random = generate(1000, 100, 12345); jobs.insert(jobs.end(), random.begin(), random.end());
            std::vector<Result> serial(jobs.size()), parallel(jobs.size());
            execute(jobs, serial, 0, false); check(jobs, serial);
            for (int threads : {1, 2, 4, 8}) for (bool dynamic : {false, true}) {
                execute(jobs, parallel, threads, dynamic); check(jobs, parallel);
                for (std::size_t i = 0; i < jobs.size(); ++i)
                    if (parallel[i].value != serial[i].value || parallel[i].steps != serial[i].steps)
                        throw std::runtime_error("serial/parallel mismatch");
            }
            std::cout << "PASS " << jobs.size() << " jobs across serial and OpenMP configurations\n";
        } else if (mode == "bench") {
            if (argc != 6) throw std::invalid_argument("bench JOBS MAX_OPERAND REPEATS SEED");
            int max_operand = std::stoi(argv[3]);
            if (max_operand < 0 || max_operand > 1000) throw std::invalid_argument("MAX_OPERAND must be in 0..1000");
            auto jobs = generate(std::stoull(argv[2]), max_operand, std::stoul(argv[5]));
            int repeats = std::stoi(argv[4]); if (repeats < 10) throw std::invalid_argument("repeats must be at least 10");
            std::vector<Result> out(jobs.size());
            std::cout << "jobs,max_operand,seed,mode,schedule,threads,repeat,seconds,steps\n";
            for (int threads : {0, 1, 2, 4, 8}) for (int sched = 0; sched < (threads ? 2 : 1); ++sched) {
                execute(jobs, out, threads, sched); check(jobs, out); // warm-up
                for (int rep = 0; rep < repeats; ++rep) {
                    double start = omp_get_wtime(); execute(jobs, out, threads, sched);
                    double seconds = omp_get_wtime() - start;
                    check(jobs, out);
                    std::uint64_t steps = 0; for (const auto& r : out) steps += r.steps;
                    std::cout << jobs.size() << ',' << argv[3] << ',' << argv[5] << ','
                              << (threads ? "openmp" : "serial") << ',' << (sched ? "dynamic" : "static") << ','
                              << threads << ',' << rep << ',' << std::setprecision(9) << seconds << ',' << steps << '\n';
                }
            }
        } else throw std::invalid_argument("unknown mode");
    } catch (const std::exception& e) { std::cerr << "error: " << e.what() << '\n'; return 1; }
}
