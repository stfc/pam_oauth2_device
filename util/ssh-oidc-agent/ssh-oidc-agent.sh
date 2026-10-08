#!/usr/bin/expect -f
#
# Fetch a live access token from a local oidc-agent and use it to skip
# pam_oauth2_device's device flow on the remote end.
#
# Requires:
#   - oidc-agent running locally with the target account loaded
#     (see https://indigo-dc.github.io/oidc-agent/configuration/directory/)
#   - expect installed locally
#   - to get the fast path, the server's /etc/pam_oauth2_device/config.json
#     needs "oauth": { "accept_access_token": true, ... } - but this script
#     works fine against a server that leaves the flag at its default
#     (false) too, it just always falls through to the normal interactive
#     device flow in that case, same as plain ssh would.
#
# Usage: ssh-oidc-agent.sh <oidc-agent-shortname> <user@host> [port]

if {[llength $argv] < 2} {
    puts stderr "usage: [info script] <oidc-agent-shortname> <user@host> \[port\]"
    exit 1
}

set shortname [lindex $argv 0]
set target    [lindex $argv 1]
set port      [expr {[llength $argv] >= 3 ? [lindex $argv 2] : 2222}]

if {[catch {exec oidc-token $shortname} token]} {
    puts stderr "oidc-token failed for '$shortname' (is oidc-agent running and the account loaded?)"
    exit 1
}

# How long to wait for the module's access-token prompt before assuming the
# server doesn't have accept_access_token enabled and handing the keyboard
# back to the user.
set timeout 10

# -tt forces a real pty so the device-flow fallback (QR/URL prompt, shell)
# still behaves like an ordinary interactive ssh session.
spawn ssh -tt -p $port $target

expect {
    # Must match the access_token_prompt constant in src/pam_oauth2_device.cpp.
    # If that prompt text is ever changed server-side, update this to match.
    -re "OAuth2 access token" {
        send -- "$token\r"
    }
    # A stable prefix of DeviceAuthResponse::get_prompt() in the same file:
    # the server has started (or fallen back to) the normal device flow, so
    # there's nothing left to automate here - hand over immediately rather
    # than waiting out the full timeout.
    -re "Authenticate at" { }
    timeout { }
    eof {
        catch wait result
        exit [lindex $result 3]
    }
}

interact
