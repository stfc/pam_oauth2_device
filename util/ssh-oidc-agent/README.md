# ssh-oidc-agent

Client-side wrapper that fetches a live OAuth2 access token from a local
[oidc-agent](https://indigo-dc.github.io/oidc-agent/) and uses it to skip
pam_oauth2_device's device flow on the server, provided the server has
`"oauth": {"accept_access_token": true}` set in its configuration (see
[HOWTO.md](../../HOWTO.md#skipping-the-device-flow-with-a-pre-fetched-oidc-agent-token)).

## Dependencies

- `oidc-agent`, running locally with the target account already loaded
- `expect`

## Run

```
./ssh-oidc-agent.sh <oidc-agent-shortname> user@host [port]
```

It fetches the token with `oidc-token <shortname>`, then uses `expect` to answer only the
module's new access-token prompt automatically. If that prompt doesn't show up within a few
seconds (eg the server doesn't have `accept_access_token` enabled), or the token is rejected,
control is handed back to you for the normal interactive device flow - `expect` mirrors all
ssh output to your terminal live the whole time, so nothing is ever silently hidden while it
waits.

## Notes

The prompt text the script matches against must match the `access_token_prompt` constant in
`src/pam_oauth2_device.cpp`; if that is ever reworded server-side, update the `expect` pattern
in this script too.
