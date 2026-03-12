#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <vector>

namespace turbot::core::session {

/// 剪枝配置 — 对齐 opencode SessionCompaction.prune() 策略
struct TURBOT_CORE_API PruneConfig {
    /// Token 数量，低于此值的近期工具结果受到保护，不会被剪枝
    int64_t protect_tokens = 40'000;

    /// 最小剪枝收益（token 数），低于此值时不执行剪枝（避免无效压缩）
    int64_t minimum_prune = 20'000;

    /// 始终豁免剪枝的工具名称列表（e.g. "skill"）
    std::vector<std::string> exempt_tools = {"skill"};
};

/// 剪枝结果
struct TURBOT_CORE_API PruneResult {
    int pruned_parts = 0;       ///< 已剪枝的 Part 数量
    int64_t freed_tokens = 0;   ///< 释放的 token 估算数
    bool did_prune = false;     ///< 是否实际执行了剪枝（false = minimum_prune 不足）
};

/// 压缩配置
struct TURBOT_CORE_API CompactionConfig {
    double overflow_threshold = 0.9;    ///< 溢出阈值（上下文使用率）
    double target_ratio = 0.5;          ///< 目标压缩比例
    int min_messages_to_keep = 5;       ///< 最少保留消息数
    bool include_tool_results = true;   ///< 是否包含工具结果
    int max_summary_length = 2000;      ///< 摘要最大长度

    /// Validate configuration
    [[nodiscard]] bool validate() const noexcept;
};

/// 压缩结果
struct TURBOT_CORE_API CompactionResult {
    std::string summary;                      ///< 摘要内容
    std::vector<std::string> retained_ids;    ///< 保留的消息 ID
    std::vector<std::string> removed_ids;     ///< 移除的消息 ID
    int64_t original_tokens = 0;              ///< 原始 token 数
    int64_t compressed_tokens = 0;            ///< 压缩后 token 数
    double compression_ratio = 0.0;           ///< 压缩比例

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// 消息重要性评分
struct TURBOT_CORE_API MessageImportance {
    std::string message_id;
    double score = 0.0;           ///< 重要性分数（0-1）
    bool has_tool_call = false;   ///< 是否包含工具调用
    bool has_tool_result = false; ///< 是否包含工具结果
    bool is_system = false;       ///< 是否为系统消息
    int position = 0;             ///< 在消息列表中的位置
};

/// 会话压缩器
class TURBOT_CORE_API SessionCompaction {
public:
    /// 剪枝旧工具输出以减少 token 消耗（对齐 opencode SessionCompaction.prune()）
    ///
    /// 策略：
    ///  - 从最新工具结果向前扫描，累积 token；
    ///  - 一旦累积超过 protect_tokens，其余旧工具结果标记为 compacted；
    ///  - PruneConfig.exempt_tools 中列出的工具名始终豁免；
    ///  - 若本次可释放 token < minimum_prune，则跳过（did_prune=false）。
    ///
    /// @param messages   当前会话消息列表（将被原地修改）
    /// @param config     剪枝配置
    /// @return           剪枝结果（包含剪枝数量和释放 token 数）
    [[nodiscard]] static PruneResult prune(
        std::vector<Message>& messages,
        const PruneConfig& config = {}
    );
    /// 检查是否需要压缩
    /// @param current_tokens 当前 token 数
    /// @param max_tokens 最大 token 数
    /// @param config 压缩配置
    /// @return true 如果需要压缩
    [[nodiscard]] static bool needs_compaction(
        int64_t current_tokens,
        int64_t max_tokens,
        const CompactionConfig& config = {}
    );

    /// 执行压缩
    /// @param messages 消息列表
    /// @param config 压缩配置
    /// @return 压缩结果
    [[nodiscard]] CompactionResult compact(
        const std::vector<Message>& messages,
        const CompactionConfig& config = {}
    );

    /// 生成摘要
    /// @param messages 要生成摘要的消息
    /// @param max_length 最大长度
    /// @return 摘要文本
    [[nodiscard]] std::string generate_summary(
        const std::vector<Message>& messages,
        int max_length = 2000
    ) const;

    /// 选择保留消息
    /// @param messages 消息列表
    /// @param config 压缩配置
    /// @return 保留的消息索引
    [[nodiscard]] std::vector<size_t> select_retained(
        const std::vector<Message>& messages,
        const CompactionConfig& config
    ) const;

    /// 估算消息 token 数
    /// @param msg 消息
    /// @return 估算的 token 数
    [[nodiscard]] static int64_t estimate_tokens(const Message& msg);

    /// 估算文本 token 数
    /// @param text 文本
    /// @return 估算的 token 数
    [[nodiscard]] static int64_t estimate_text_tokens(const std::string& text);

    /// 设置摘要生成回调
    /// @param callback 回调函数（用于 LLM 生成摘要）
    void set_summary_generator(
        std::function<std::string(const std::vector<Message>&)> callback
    );

private:
    std::function<std::string(const std::vector<Message>&)> summary_generator_;

    /// 计算消息重要性
    [[nodiscard]] MessageImportance calculate_importance(
        const Message& msg,
        size_t position,
        size_t total
    ) const;

    /// 按重要性排序消息
    [[nodiscard]] std::vector<MessageImportance> rank_messages(
        const std::vector<Message>& messages
    ) const;

    /// 创建摘要消息
    [[nodiscard]] Message create_summary_message(
        const std::string& summary,
        const std::vector<std::string>& removed_ids
    ) const;
};

} // namespace turbot::core::session
