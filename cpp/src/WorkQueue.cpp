#include "WorkQueue.h"

WorkQueue::WorkQueue(size_t cap) : cap_(cap), ring_(cap) {}

void WorkQueue::push(Item item) {
  std::unique_lock<std::mutex> lk(mu_);
  cvSpace_.wait(lk, [&] { return count_ < cap_; });
  ring_[tail_] = item;
  tail_ = (tail_ + 1) % cap_;
  ++count_;
  cvWork_.notify_one();
}

std::optional<WorkQueue::Item> WorkQueue::pop() {
  std::unique_lock<std::mutex> lk(mu_);
  cvWork_.wait(lk, [&] { return count_ > 0 || closed_; });
  if (count_ == 0) return std::nullopt;
  Item item = ring_[head_];
  head_ = (head_ + 1) % cap_;
  --count_;
  cvSpace_.notify_one();
  return item;
}

void WorkQueue::close() {
  std::unique_lock<std::mutex> lk(mu_);
  closed_ = true;
  cvWork_.notify_all();
}
