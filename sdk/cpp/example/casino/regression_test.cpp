/**
 * Copyright 2018-2020 Pejman Ghorbanzade. All rights reserved.
 */

#include "boost/format.hpp"
#include "boost/lexical_cast.hpp"
#include "cxxopts.hpp"
#include "example/casino/code_under_test.hpp"
#include "weasel/devkit/filesystem.hpp"
#include "weasel/weasel.hpp"
#include <iostream>
#include <thread>

using Options = std::map<std::string, std::string>;

/**
 *
 */
std::vector<weasel::casino::Table> parse_file(
    const boost::filesystem::path& path)
{
    std::ifstream ifile(path.string());
    if (!ifile.is_open()) {
        throw std::invalid_argument(
            "failed to open input file: " + path.string());
    }
    auto num = 0u;
    std::string line;
    std::string text;
    std::vector<weasel::casino::Table> tables;
    while (std::getline(ifile, line)) {
        ++num;
        text += line + '\n';
        if (num % 6) {
            continue;
        }
        weasel::casino::Table table;
        std::istringstream iss(text);
        text.clear();
        iss >> table;
        tables.push_back(table);
    }
    ifile.close();
    return tables;
}

/**
 *
 */
bool simulate_table(weasel::casino::Table& table, const Options& opts)
{
    using policy_t = weasel::casino::Policy::Type;
    const std::map<std::string, policy_t>& policies = {
        { "default", policy_t::Default }, { "simple", policy_t::Simple }
    };
    try {
        table.setPolicy(policies.at(opts.at("policy")));
        weasel::declare_testcase(table.name());
        weasel::add_assertion("original_state", table);
        for (auto i = 0u; i < 13; i++) {
            std::cout << "Round: " << i + 1 << std::endl;
            const auto result = table.playRound();
            boost::format fmt { "round_%02d" };
            fmt % (i + 1);
            weasel::add_result(fmt.str(), result);
        }
        weasel::add_result("final_state", table);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return false;
    }
    return true;
}

enum EMode : unsigned char {
    kGenerate,
    kExecute,
    kTest,
    kInvalid
};

struct OperationBase {
    virtual bool run() const = 0;
    virtual bool validate() const = 0;
    virtual ~OperationBase() = default;
};

template <EMode E>
class Operation : public OperationBase {
public:
    Operation(const Options& opts)
        : _opts(opts)
    {
    }
    virtual ~Operation() = default;
    bool run() const;
    bool validate() const;

protected:
    const Options _opts;
};

template <>
bool Operation<kGenerate>::run() const
{
    const auto& print = [](const unsigned int num, std::ostream& out) {
        std::set<weasel::casino::Table> tables;
        while (tables.size() < num) {
            weasel::casino::Table table;
            table.generate();
            tables.insert(table);
        }
        for (const auto& table : tables) {
            out << table << std::endl;
        }
    };
    const auto num = boost::lexical_cast<unsigned int>(_opts.at("count"));
    if (!_opts.count("output")) {
        print(num, std::cout);
        return true;
    }
    const auto output = boost::filesystem::absolute(_opts.at("output"));
    if (!weasel::filesystem::exists(output.parent_path().string())
        && !boost::filesystem::create_directories(output.parent_path())) {
        std::cerr << "failed to create directory: " << output.string()
                  << std::endl;
        return false;
    }
    std::ofstream file(output.string(), std::ios::trunc);
    print(num, file);
    file.close();
    return true;
}

template <>
bool Operation<kGenerate>::validate() const
{
    if (!_opts.count("count")) {
        return false;
    }
    const auto& num = boost::lexical_cast<unsigned int>(_opts.at("count"));
    if (100 < num) {
        std::cerr << "maximum number of testcases to generate is 100"
                  << std::endl;
        return false;
    }
    return true;
}

template <>
bool Operation<kExecute>::run() const
{
    for (auto& table : parse_file(_opts.at("input"))) {
        if (!simulate_table(table, _opts)) {
            std::cerr << "failed to simulate table " << table.name()
                      << std::endl;
            return false;
        }
    }
    return true;
}

template <>
bool Operation<kExecute>::validate() const
{
    // check for missing config options
    for (const auto& key : { "input", "policy" }) {
        if (!_opts.count(key)) {
            std::cerr << "missing required option: " << key << std::endl;
            return false;
        }
    }
    const auto& file = _opts.at("input");
    if (!weasel::filesystem::is_regular_file(file)) {
        std::cerr << "specified input file is missing" << std::endl;
        return false;
    }
    return true;
}

template <>
bool Operation<kTest>::run() const
{
    weasel::configure({ { "api-key", _opts.at("api-key") },
        { "api-url", _opts.at("api-url") },
        { "version", _opts.at("version") } });

    if (!weasel::is_configured()) {
        std::cerr << weasel::configuration_error() << std::endl;
        return EXIT_FAILURE;
    }

    auto delay = 0u;
    if (_opts.count("delay")) {
        boost::conversion::try_lexical_convert<unsigned>(
            _opts.at("delay"), delay);
    }

    for (auto& table : parse_file(_opts.at("input"))) {
        if (!simulate_table(table, _opts)) {
            std::cerr << "failed to simulate table " << table.name()
                      << std::endl;
            return false;
        }
        // if user has specified an explicit delay, submit each testcase
        // individually and wait after simulating each table.
        if (0u != delay) {
            if (!weasel::post()) {
                std::cerr << "failed to submit testresults to weasel platform"
                          << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    // if user has not specified an explicit delay, submit all testcases
    // together in one post operation.
    if (0u == delay && !weasel::post()) {
        std::cerr << "failed to submit testresults to weasel platform"
                  << std::endl;
    }

    // we can also store testresults to disk if we like to.
    if (_opts.count("output")) {
        weasel::save_json(_opts.at("output"));
    }
    return true;
}

template <>
bool Operation<kTest>::validate() const
{
    // validate config parameters needed to run simulation
    Operation<kExecute> op(_opts);
    if (!op.validate()) {
        return false;
    }
    // check for missing config parameters needed for regression testing
    for (const auto& key : { "delay", "api-key", "api-url", "version" }) {
        if (!_opts.count(key)) {
            std::cerr << "missing required option: " << key << std::endl;
            return false;
        }
    }
    // also check that provided delay is an unsigned integer
    unsigned delay = 0;
    if (!boost::conversion::try_lexical_convert<unsigned>(
            _opts.at("delay"), delay)) {
        std::wcerr << "delay must be an unsigned integer" << std::endl;
        return false;
    }
    return true;
}

cxxopts::Options get_options_description(const EMode mode)
{
    // clang-format off
    cxxopts::Options opts_basic("Options");
    opts_basic.add_options()
        ("h,help", "displays this help message")
        ("mode", "operational mode of this application [new|run|test]", cxxopts::value<std::string>());

    cxxopts::Options opts_new("Options --mode=new");
    opts_new.add_options()
        ("count", "generates one or more testcases", cxxopts::value<std::string>()->default_value("1"))
        ("output", "path to file to write generated testcases into", cxxopts::value<std::string>());

    cxxopts::Options opts_run("Options --mode=run");
    opts_run.add_options()
        ("input", "path to file to read testcases from", cxxopts::value<std::string>())
        ("policy", "code under test implementation variant", cxxopts::value<std::string>()->default_value("default"));

    cxxopts::Options opts_test("Options --mode=test");
    opts_test.add_options()
        ("input", "path to file to read testcases from", cxxopts::value<std::string>())
        ("output", "path to file to write testresults into", cxxopts::value<std::string>())
        ("policy", "code under test implementation variant", cxxopts::value<std::string>()->default_value("default"))
        ("delay", "time to wait in milliseconds before submitting result for each testcase", cxxopts::value<std::string>()->default_value("0"))
        ("api-key", "Weasel API key", cxxopts::value<std::string>())
        ("api-url", "root url of Weasel platform", cxxopts::value<std::string>())
        ("version", "testsuite version", cxxopts::value<std::string>());
    // clang-format on

    // if operational mode is known, attempt to parse command line arguments
    // specific to that operational mode
    std::map<EMode, const cxxopts::Options&> descs {
        { kGenerate, opts_new },
        { kExecute, opts_run },
        { kTest, opts_test },
        { kInvalid, opts_basic }
    };
    return descs.at(mode);
}

EMode detect_operational_mode(const Options& opts)
{
    if (!opts.count("mode")) {
        return kInvalid;
    }
    const auto& mode = opts.at("mode");
    std::map<std::string, EMode> modes { { "new", kGenerate },
        { "run", kExecute },
        { "test", kTest } };
    return modes.count(mode) ? modes.at(mode) : kInvalid;
}

/**
 * Parses command line arguments.
 *
 * @param argc number of command line arguments
 * @param argv values of command line arguments
 * @param options reference to the container representing program options
 * @return zero if all command line arguments were parsed successfully
 *         positive if application should exit immediately with success
 *         negative if application should exit immediately with failure
 */
int read_command_line_options(int argc, char* argv[], Options& options)
{
    // perform initial pass on provided command line arguments
    // and detect the operational mode

    auto opts_basic = get_options_description(kInvalid);
    opts_basic.allow_unrecognised_options();
    const auto& results_basic = opts_basic.parse(argc, argv);

    if (results_basic.count("mode")) {
        options["mode"] = results_basic["mode"].as<std::string>();
    }
    const auto& mode = detect_operational_mode(options);
    auto opts_mode = get_options_description(mode);
    opts_mode.allow_unrecognised_options();
    const auto& results_mode = opts_mode.parse(argc, argv);

    // if user gives --help argument, print appropriate help message and exit
    if (results_mode.count("help")) {
        std::cout << opts_mode.help() << std::endl;
        return 1;
    }
    // // update options object with set of all recognizable keys
    const auto& keys = { "mode", "count", "config", "delay", "input",
        "output", "policy", "api-key", "api-url", "version" };
    for (const auto& key : keys) {
        if (results_mode.count(key)) {
            options[key] = results_mode[key].as<std::string>();
        }
    }
    // if operational mode is invalid, print help message and exit
    if (kInvalid == mode) {
        std::cout << opts_mode.help() << std::endl;
        return -1;
    }
    return 0;
}

/**
 *
 *
 * @param argc
 * @param argv
 */
int main(int argc, char* argv[])
{
    Options options;

    // update options based on given command lines
    // the function handles cases where user has used options such as
    // --help or --version or has provided invalid options in which case
    // non-zero value is returned as a signal to end program execution.
    const auto ret = read_command_line_options(argc, argv, options);
    if (ret) {
        return ret;
    }
    const auto& mode = detect_operational_mode(options);
    std::map<EMode, std::shared_ptr<OperationBase>> ops {
        { kGenerate, std::make_shared<Operation<kGenerate>>(options) },
        { kExecute, std::make_shared<Operation<kExecute>>(options) },
        { kTest, std::make_shared<Operation<kTest>>(options) }
    };
    if (!ops.count(mode)) {
        std::cerr << "operation is not implemented" << std::endl;
        std::cerr << get_options_description(kInvalid).help() << std::endl;
        return EXIT_FAILURE;
    }
    const auto& operation = ops.at(mode);
    // check that configuration parameters are valid for given operation
    if (!operation->validate()) {
        std::cerr << "option validation failed" << std::endl;
        std::cerr << get_options_description(mode).help() << std::endl;
        return EXIT_FAILURE;
    }
    // perform the operation
    if (!operation->run()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
