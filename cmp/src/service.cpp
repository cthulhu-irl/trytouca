/**
 * Copyright 2018-2020 Pejman Ghorbanzade. All rights reserved.
 */

#include "service.hpp"
#include "boost/filesystem.hpp"
#include "flatbuffers/flatbuffers.h"
#include "rapidjson/schema.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "spdlog/spdlog.h"
#include "weasel/devkit/comparison.hpp"
#include "weasel/devkit/platform.hpp"
#include "weasel/devkit/resultfile.hpp"
#include "weasel/devkit/testcase.hpp"
#include "weasel/devkit/utils.hpp"
#include <iostream>
#include <memory>
#include <thread>

#define pov boost::program_options::value<std::string>
using co = ConfigOptions::Value;

/**
 *
 */
ConfigOptions::ConfigOptions()
    : data({ { Value::api_url, "api-url" },
             { Value::log_dir, "log-dir" },
             { Value::log_level, "log-level" },
             { Value::max_failures, "max-failures" },
             { Value::project_dir, "project-dir" },
             { Value::sleep_interval, "sleep-interval" },
             { Value::startup_attempt_interval, "startup-attempt-interval" },
             { Value::startup_max_attempts, "startup-max-attempts" },
             { Value::storage_dir, "storage-dir" } })
{
}

/**
 *
 */
boost::program_options::options_description ConfigOptions::description() const
{
    boost::program_options::options_description opts_file {
        "Configuration File Options"
    };
    // clang-format off
    opts_file.add_options()
        ("api-url", pov(), "URL to Weasel Platform API")
        ("log-dir",
            pov(),
            "path, relative to project directory, to the directory "
            "to write log files into")
        ("log-level", pov()->default_value("info"),
            "level of details to use for logging")
        ("max-failures",
            pov()->default_value("10"),
            "number of allowable consecutive failures before we conclude "
            "that comparator has encountered a fatal issue")
        ("project-dir", pov(), "full path to project root directory")
        ("sleep-interval",
            pov()->default_value("10"),
            "minimum time (s) before re-polling database for unprocessed "
            "comparison jobs")
        ("startup-attempt-interval",
            pov()->default_value("12000"),
            "minimum time (ms) to wait before attempting to rerun "
            "startup stage")
        ("startup-max-attempts",
            pov()->default_value("10"),
            "maximum number of attempts to run startup stage")
        ("storage-dir",
            pov(),
            "path, relative to project directory, to the directory to "
            "store result files into");
    // clang-format on
    return opts_file;
}

/**
 *
 */
cxxopts::Options ConfigOptions::config_file_options() const
{
    cxxopts::Options opts_file("configuration file parameters");

    // clang-format off
    opts_file.add_options()
        ("api-url", "url to weasel platform api", cxxopts::value<std::string>())
        ("log-dir", "relative path to log directory", cxxopts::value<std::string>())
        ("log-level", "level of detail to use for logging", cxxopts::value<std::string>()->default_value("info"))
        ("max-failures", "number of allowable consecutive failures", cxxopts::value<std::string>()->default_value("10"))
        ("project-dir", "full path to project root directory", cxxopts::value<std::string>())
        ("sleep-interval", "minimum time (s) before re-polling database for unprocessed comparison jobs", cxxopts::value<std::string>()->default_value("10"))
        ("startup-attempt-interval", "minimum time (ms) to wait before attempting to rerun startup stage", cxxopts::value<std::string>()->default_value("12000"))
        ("startup-max-attempts", "maximum number of attempts to run startup stage", cxxopts::value<std::string>()->default_value("10"))
        ("storage-dir", "relative path to weasel data store", cxxopts::value<std::string>());
    // clang-format on

    return opts_file;
}

/**
 *
 */
Service::Service(const ConfigOptions& opts)
    : _opts(opts.data)
{
}

/**
 * this function is called from a context in which logger may not
 * have been initialized yet. it is safe to write errors in standard
 * error instead.
 */
bool Service::validate() const
{
    // check no required option is missing
    const auto& missingKeys = _opts.findMissingKeys({ co::api_url, co::project_dir, co::storage_dir });
    if (!missingKeys.empty())
    {
        fmt::print(stderr, "cannot continue when required keys are missing\n");
        fmt::print(stderr, "missing required option(s):\n");
        for (const auto& key : missingKeys)
        {
            if (!_opts.hasNameForKey(key))
            {
                fmt::print(stderr, " - unknown ({})\n", static_cast<int>(key));
                continue;
            }
            fmt::print(stderr, " - {}\n", _opts.toName(key));
        }
        return false;
    }

    return true;
}

/**
 *
 */
bool Service::init()
{
    if (!runStartupStage())
    {
        spdlog::error("failed during start-up stage");
        return false;
    }

    return true;
}

/**
 *
 */
bool Service::runStartupStage() const
{
    const auto& maxAttempts = _opts.get<unsigned int>(co::startup_max_attempts);
    const auto& interval = std::chrono::milliseconds(_opts.get<unsigned int>(co::startup_attempt_interval));
    spdlog::info("running start-up stage");
    weasel::ApiUrl apiUrl(_opts.get(co::api_url));
    weasel::ApiConnector apiConnector(apiUrl);
    for (auto i = 1u; i <= maxAttempts; ++i)
    {
        if (apiConnector.handshake())
        {
            spdlog::info("start-up phase completed");
            return true;
        }
        spdlog::warn("running start-up stage: attempt ({}/{})", i, maxAttempts);
        std::this_thread::sleep_for(interval);
    }
    return false;
}

/**
 *
 */
bool Service::run() const
{
    namespace chr = std::chrono;
    weasel::ApiUrl apiUrl(_opts.get(co::api_url));
    weasel::ApiConnector apiConnector(apiUrl);

    spdlog::info("hello from weasel comparator");
    spdlog::info("starting to run comparator in service mode");

    const auto& interval = chr::duration_cast<chr::milliseconds>(
        chr::seconds(_opts.get<unsigned int>(co::sleep_interval)));

    while (true)
    {
        const auto& jobs = apiConnector.getComparisonList();
        if (!jobs.empty() && !runTask(jobs))
        {
            spdlog::warn("failed to perform periodic operation");
            break;
        }
        if (jobs.empty())
        {
            std::this_thread::sleep_for(interval);
        }
    }

    return false;
}

/**
 * since the comparator is running as a service, we allow it to
 * fail to process one or multiple comparison jobs in hope that
 * it can re-process those jobs in a subsequent execution cycle.
 * at the same time, if the comparator fails to process multiple
 * consecutive jobs, it may be indicative of a serious problem,
 * in which case we gracefully abort the execution of this process
 * in order to prevent potential corruption of multiple jobs.
 */
bool Service::runTask(const std::vector<weasel::ComparisonJob>& jobs) const
{
    namespace chr = std::chrono;
    const auto& tic = chr::system_clock::now();
    spdlog::info("processing {} comparison jobs", jobs.size());

    // find maximum number of consecutive failures of the comparator
    // that should trigger graceful termination of the process.

    const auto maxFailures = std::min(
        static_cast<unsigned>(jobs.size()),
        _opts.get<unsigned>(co::max_failures));

    // process comparison jobs one by one

    auto failureCount = 0u;
    for (const auto& job : jobs)
    {
        if (maxFailures < failureCount)
        {
            spdlog::error("exceeded maximum consecutive failures");
            return false;
        }
        if (!processJobAttempt(job))
        {
            spdlog::error("{}: failed to process comparison job", job.id);
            ++failureCount;
            continue;
        }
        failureCount = 0;
    }

    const auto& toc = chr::system_clock::now();
    const auto& dur = chr::duration_cast<chr::milliseconds>(toc - tic);
    spdlog::info("processed {} comparison jobs: ({} ms)", jobs.size(), dur.count());
    spdlog::info("average processing time: {:.2f} ms per job", _stats.avg);
    return true;
}

/**
 *
 */
bool Service::processJobAttempt(const weasel::ComparisonJob& job) const
{
    namespace chr = std::chrono;
    const auto& tic = chr::system_clock::now();
    spdlog::debug("{}: processing comparison job", job.id);

    const auto dst = loadResultFile(job.dstBatch, job.dstMessage);
    const auto src = loadResultFile(job.srcBatch, job.srcMessage);

    if (!src || !dst)
    {
        spdlog::warn("{}: comparison job is orphaned", job.id);
        return false;
    }

    const auto& dstName = src->metadata().describe();
    const auto& srcName = dst->metadata().describe();

    // ensure dst message is processed

    if (!job.dstProcessed && !processMessage(dst, job.dstMessage))
    {
        spdlog::error("{}: failed to process message", dstName);
        return false;
    }

    // ensure src message is processed

    if (job.dstMessage != job.srcMessage && !job.srcProcessed
        && !processMessage(src, job.srcMessage))
    {
        spdlog::error("{}: failed to process message", srcName);
        return false;
    }

    // now process the comparison job

    if (!processComparisonJob(dst, src, job.id))
    {
        spdlog::error("{}: failed to compare with {}", dstName, srcName);
        return false;
    }

    // log that comparison job was processed

    const auto& toc = chr::system_clock::now();
    const auto& dur = chr::duration_cast<chr::milliseconds>(toc - tic);
    spdlog::info("{}: compared with {} ({} ms)", dstName, srcName, dur.count());
    _stats.update(dur.count());
    return true;
}

/**
 *
 */
std::shared_ptr<weasel::Testcase> Service::loadResultFile(
    const std::string& batchId,
    const std::string messageId) const
{
    boost::filesystem::path fullpath(_opts.get(co::project_dir));
    fullpath /= _opts.get(co::storage_dir);
    fullpath /= batchId;
    fullpath /= messageId;

    // check that the result file exists

    if (!boost::filesystem::is_regular_file(fullpath))
    {
        spdlog::error("{}: result file is missing", fullpath.string());
        return nullptr;
    }

    // read binary data of stored result file and attempt to parse it
    // into a TestCase object.

    const auto binaryContent = weasel::load_string_file(fullpath.string());
    std::vector<uint8_t> buffer(binaryContent.begin(), binaryContent.end());
    try
    {
        return std::make_shared<weasel::Testcase>(buffer);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("{}: failed to parse result: {}", fullpath.string(), ex.what());
        return nullptr;
    }
}

/**
 *
 */
bool Service::processMessage(
    const std::shared_ptr<weasel::Testcase>& result,
    const std::string& messageId) const
{
    const auto& desc = result->metadata().describe();
    spdlog::debug("{}: processing message", desc);

    // create json output to be posted to backend

    rapidjson::Document doc(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("overview", result->overview().json(allocator), allocator);
    doc.AddMember("body", result->json(allocator), allocator);

    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    writer.SetMaxDecimalPlaces(3);
    doc.Accept(writer);
    const auto output = strbuf.GetString();

    // submit output to Weasel Platform

    weasel::ApiUrl apiUrl(_opts.get(co::api_url));
    weasel::ApiConnector apiConnector(apiUrl);

    if (!apiConnector.processMessage(messageId, output))
    {
        spdlog::warn("{}: failed to submit message", desc);
        return false;
    }

    spdlog::debug("{}: processed message", desc);
    return true;
}

/**
 *
 */
bool Service::processComparisonJob(
    const std::shared_ptr<weasel::Testcase>& dst,
    const std::shared_ptr<weasel::Testcase>& src,
    const std::string& jobId) const
{
    const auto& dstName = dst->metadata().describe();
    const auto& srcName = src->metadata().describe();
    const auto& tuple = weasel::format("{}_{}", dstName, srcName);
    spdlog::debug("{}: processing comparison job", tuple);

    // perform comparison

    const auto& result = src->compare(dst);

    // create json output to be posted to backend

    rapidjson::Document doc(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("overview", result.overview().json(allocator), allocator);
    doc.AddMember("body", result.json(allocator), allocator);

    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    writer.SetMaxDecimalPlaces(3);
    doc.Accept(writer);
    const auto output = strbuf.GetString();

    // submit output to Weasel Platform

    weasel::ApiUrl apiUrl(_opts.get(co::api_url));
    weasel::ApiConnector apiConnector(apiUrl);

    if (!apiConnector.processComparison(jobId, output))
    {
        spdlog::warn("{}: failed to submit comparison job", tuple);
        return false;
    }

    spdlog::debug("{}: processed comparison job", tuple);
    return true;
}
