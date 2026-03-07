#include <turbot/storage/transaction.hpp>
#include <nlohmann/json.hpp>

namespace turbot::storage {

// TransactionGuard implementation

TransactionGuard::TransactionGuard(std::shared_ptr<Transaction> tx)
    : tx_(std::move(tx)) {}

TransactionGuard::~TransactionGuard() {
    if (tx_ && tx_->is_active()) {
        try {
            tx_->rollback();
        } catch (...) {
            // Ignore rollback errors in destructor
        }
    }
}

TransactionGuard::TransactionGuard(TransactionGuard&& other) noexcept
    : tx_(std::move(other.tx_)) {}

TransactionGuard& TransactionGuard::operator=(TransactionGuard&& other) noexcept {
    if (this != &other) {
        if (tx_ && tx_->is_active()) {
            try {
                tx_->rollback();
            } catch (...) {
                // Ignore rollback errors
            }
        }
        tx_ = std::move(other.tx_);
    }
    return *this;
}

void TransactionGuard::commit() {
    if (tx_) {
        tx_->commit();
        tx_.reset();
    }
}

void TransactionGuard::rollback() {
    if (tx_) {
        tx_->rollback();
        tx_.reset();
    }
}

Transaction* TransactionGuard::operator->() {
    return tx_.get();
}

Transaction& TransactionGuard::operator*() {
    return *tx_;
}

TransactionGuard::operator bool() const {
    return tx_ != nullptr;
}

std::shared_ptr<Transaction> TransactionGuard::release() {
    return std::move(tx_);
}

} // namespace turbot::storage
