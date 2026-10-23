#include "charger/LineProtocol.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace charger {

namespace {

std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

Command invalid(std::string reason) {
    Command command;
    command.reason = std::move(reason);
    return command;
}

Command simple(Command::Kind kind) {
    Command command;
    command.kind = kind;
    return command;
}

// "CURRENT 16" or "CURRENT 7.5": one decimal number, within the cable rating.
Command parseCurrent(std::string_view argument) {
    if (argument.empty()) {
        return invalid("CURRENT needs a value in amperes");
    }
    // strtod needs a terminated string; the argument is a view into the line.
    const std::string text(argument);
    char* end = nullptr;
    errno = 0;
    const double amperes = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || errno != 0 || !std::isfinite(amperes)) {
        return invalid("CURRENT value is not a number");
    }
    if (amperes < 0.0 || amperes > kMaxRequestedCurrentA) {
        return invalid("CURRENT value out of range 0 to 80");
    }
    Command command = simple(Command::Kind::Current);
    command.amperes = amperes;
    return command;
}

}  // namespace

Command parseCommand(std::string_view line) {
    const std::string_view text = trim(line);
    if (text.empty()) {
        return invalid("empty line");
    }
    // Split at the first space: the word, then whatever follows it.
    const auto space = text.find(' ');
    const std::string_view word = text.substr(0, space);
    const std::string_view argument =
        space == std::string_view::npos ? std::string_view{} : trim(text.substr(space + 1));
    if (word == "CURRENT") {
        return parseCurrent(argument);
    }
    // TODO - preferably use a map of string to Command::Kind, but this is simple enough for now.
    if (!argument.empty()) {
        return invalid("unexpected argument");
    }
    if (word == "PLUG") {
        return simple(Command::Kind::Plug);
    }
    if (word == "UNPLUG") {
        return simple(Command::Kind::Unplug);
    }
    if (word == "AUTH") {
        return simple(Command::Kind::Auth);
    }
    if (word == "READY") {
        return simple(Command::Kind::Ready);
    }
    if (word == "SUSPEND") {
        return simple(Command::Kind::Suspend);
    }
    if (word == "FAULT") {
        return simple(Command::Kind::Fault);
    }
    if (word == "CLEAR") {
        return simple(Command::Kind::Clear);
    }
    if (word == "STATE") {
        return simple(Command::Kind::State);
    }
    return invalid("unknown command");
}

}  // namespace charger