# PAM module for OAuth 2.0 Device flow

This is a PAM module that lets you log in via SSH to servers using OpenID Connect credentials, instead of SSH Keys or a username and password combination.

It uses the OAuth2 Device Flow, which means that during the login process, you will click a link and log in to your OpenID Connect Provider, which will then authenticate you for the SSH session. 

This module will then check if you're in the right group(s) or have a specified username, and allow or deny access.

A demo video is avaliable here: https://drive.google.com/file/d/1WzDRL0RFDXfvUgabbXNzBppV-DKXyUN1/view?usp=sharing

This code was originally developed by [Mazarykova Univerzita](https://github.com/ICS-MU/pam_oauth2_device) and [this branch](https://github.com/stfc/pam_oauth2_device) has been refactored by UKRI-STFC as a part of the [IRIS](https://www.iris.ac.uk/) activity.  This work was funded by [STFC](https://www.ukri.org/councils/stfc/), a part of [UK Research and Innovation (UKRI)](https://www.ukri.org/).


## Build

The upstream build uses basic `make` and we have stuck with this for compatibility reasons.
The two basic targets are `make` and `make test`; the latter will build (some of) the tests and run them.

* Note that at present some of the integration tests are failing.
* Note you can build RPMs and DEBs, though currently they are designed for CentOS7 and Ubuntu 18.04 respectively.
  * The build currently requires Docker (or compatible)

### Manual build on Rocky 8

From a base Rocky 8 system (tested with 8.10)
```
sudo dnf install gcc gcc-c++ make git
sudo dnf install pam-devel openldap-devel libcurl-devel
git clone https://github.com/stfc/pam_oauth2_device.git
cd pam_oauth2_device
make 
```

If you also want  `pamtester` (for testing, see below)

```
sudo dnf install epel-release
sudo dnf update
sudo dnf install pamtester
```


### Manual Build on Scientific Linux or CentOS7

```
yum install epel-release
yum install openldap-devel
yum install libcurl-devel
yum install pam-devel
yum install libldb-devel
yum install http://ftp.scientificlinux.org/linux/scientific/7x/external_products/softwarecollections/yum-conf-softwarecollections-2.0-1.el7.noarch.rpm
yum install devtoolset-8 # this is needed as we need a more up to date g++ version than is supplied by default in SL repos.
scl enable devtoolset-8 bash
git clone https://github.com/stfc/pam_oauth2_device.git
cd pam_oauth2_device/
make
cp pam_oauth2_device.so /lib64/security/pam_oauth2_device.so
cp config_template.json config.json
```

### Build on Debian 13 or Ubuntu (22.04 LTS or )

```
apt install libpam0g-dev
apt install libcurl4-openssl-dev
apt install libldap-dev
git clone https://github.com/stfc/pam_oauth2_device.git
cd pam_oauth2_device/
make
```
Similarly, run `apt install pamtester` to get the tester, if required (see below)

## Installation

To install the module, copy `pam_oauth2_device.so` into the PAM modules directory (usually with permissions 0755).

On Debian-based systems, this would be `/lib/x86_64-linux-gnu/security` whereas CentOS and related flavours would use `/usr/lib64/security`.  If in doubt, check `dpkg --L libpam-modules` or `rpm -ql pam` respectively.

It is possible to just type `make install` and the installer is now fixed to the correct location on Rocky/CentOS.
It still [overwrites the configuration](https://github.com/stfc/pam_oauth2_device/issues/14) but at least now creates a
backup.

## Configuration

Although JSON is not ideal for configuration files (it's a bit picky about syntax and it's hard to add comments), we now
support splitting the configuration file into several segments (all of which should be JSON "objects" at the outermost
level).  This is useful for maintaining parts of the file such as client secrets or user maps in separate files.

### Splitting configuration into several files

In any place where a JSON object (the curly braces construct) is expected in the configuration, a JSON string can be
placed instead with a filename in the string; this file will be loaded and will serve as the object in question and must
have the format expected in the configuration template.  This can be nested to any depth required.  It is strongly
recommended to use full pathnames inside these strings.

For example, if the normal configuration file looks like this (obviously a real configuration file would have more stuff
in it):

```
{
  "oauth": {
    "client": {
      "id": "abcd",
      "secret": "meetmeinstlouis"
  },
  "scope": "openid profile"
}
```
and the system administrator decides to store the client object in a separate file, they can do it as follows:

```
{
  "oauth": {
    "client": "/etc/pam_oauth2_device/client.json",
  "scope": "openid profile"
}
```
The `client.json` file should then look like this:
```
{
 "id": "abcd",
 "secret": "meetmeinstlouis"
}
```

In contrast, the `scope` cannot be split off into a separate file as a string is expected as value for the `scope` parameter.

### User names

Usernames are mentioned several times in this document and could probably get a bit confusing.  This section attempts to give a short explanation.

For every user there are *three* usernames, which can be distinct.

The **local user name** is the name the user uses in the ssh login, as in `ssh fred@example.com` where the local user name is `fred`.  This is the name that is passed into the PAM module for authentication.

The **remote user name** is the corresponding name for the user as held by the IAM system.  Once the user has successfully authenticated to IAM, IAM publishes a "userinfo" structure with the user's name and email address and other attributes that IAM can assert.  Within this structure, the PAM module can pick an attribute to use as the remote user name (using the `username_attribute` option).

The **account name** is the name of the local Unix account that the user is mapped into once they have authenticated.  By default, it is the same as the local user name.

### Summary of Configuration Options

The template `config_template.json` should give an outline of the configuration file.

The configuration should be installed at `/etc/pam_oauth2_device/config.json` (a future release should make it configurable, to allow different modules to coexist).

As the name suggests, the file is in JSON, so it is recommended to check it with a JSON validator like `jq` after editing it (the PAM module will refuse to load an invalid JSON file, but you will not see this error till runtime.)

Thus, at the top level, there is a single object with a number of entries, described as "sections" in the table:

#### Table 1: Configuring Authentication Flow

| Section | Entry | Type | Req'd | Description | Notes |
| --- | --- | --- | --- | --- | --- |
| oauth | | Object | Y | | |
| oauth | client | Object | Y | Contains "id" and "secret" | |
| oauth | scope | String | Y | OIDC scope | Note 1 |
| oauth | device\_endpoint | String | Y | Device endpoint | https://${url}/devicecode |
| oauth | token\_endpoint | String | Y | Token endpoint | https://${url}/token |
| oauth | userinfo\_endpoint | String | Y | Userinfo | https://${url}/userinfo |
| oauth | username\_attribute | String | Y | Attribute for remote username | |
| oauth | local\_username\_suffix | String | Y | See usernames | |
| tls | | Object | Y | | |
| tls | ca\_bundle | String | N | Concatenated list of trust anchors | Note 2 |
| tls | ca\_path | String | N | Directory with trust anchors | Note 2 |
| tls | debug | bool | N | Extra certificate debug | |
| qr | | Object | N | | |
| qr | error\_correction\_level | Int | Y | QR code | Note 3 |

Notes:

1 The string value should have a _space separated_ list of scopes which must include `offline_access`
2 If present, the "ca\_bundle" must be a file with PEM-formatted trust anchors (CA certificates) concatenated together.
   * "ca\_path" works only with OpenSSL
   * On the target system, use `curl -V` to see whether curl uses OpenSSL or NSS (or something different again)
   * If both ca\_path and ca\_bundle are present, the latter takes precedence
   * If neither is present, a compile-time system default should apply (the default has changed with curl 7.55 using the
     NSS engine)
   * curl may use a default for the unset option: i.e. if a bundle is provided, curl may use a default for the path.
     Use the tls debug option to find out what's going on if you have trouble figuring out what's happening.
   * *Warning*: some TLS engines may not support the bundle, some may not support the path.
     - Both OpenSSL and NSS should support the bundle,
	 - OpenSSL supports the path.
   * Note that prior to curl 7.56, there is no sensible way for the curl client (i.e. the PAM module) to know what
     curl's TLS engine is (though you should still run `curl -V` by hand to check)
3 The QR code section is optional but if present, it must have the error correction level defined.  Permitted values are 1 (low), 2 (medium), 3 (high) or -1 (disabled).  If the section is missing, the QR code is disabled.
4 The "${url}" above would be the URL (hostname) of your OpenID Provider.  Its host certificate must be valid when checked against the CA bundle (see item 2)

#### Table 2: Configuring Authorisation Flow

No authorisation section is required, although if included there will be necessary entries in the section.  However, if no authorisation section is provided, the user will not be able to log in.

This part of the module functionality carries a lot of legacy stuff; see the Authorisation section for discussion.

| Section | Entry | Type | Req'd | Description | Notes |
| --- | --- | --- | --- | --- | --- |
| ldap | | Object | N | LDAP configuration | |
| ldap | host | String | Y | LDAP URL | |
| ldap | basedn | String | Y | Base DN | Note 1 |
| ldap | user | String | Y | username | Note 2 |
| ldap | passwd | String | Y | password | Note 2 |
| ldap | scope | String | N | | Note 3 |
| ldap | preauth | String | N | | |
| ldap | filter | String | N | | Note 4 |
| ldap | filter\_local | String | N | | Note 4 |
| ldap | attr | String | Y | | |
| group | | Object | N | | |
| group | access | boolean | Y | Whether to use this section | |
| group | service\_name | String | Y | | Note 5 |
| cloud | | Object | N | | |
| cloud | access | boolean | Y | Whether to use this section | |
| cloud | endpoint | String | Y | | Note 6 |
| cloud | username | String | Y | endpoint username | Note 6 |
| cloud | metadata\_file | String | Y | | Note 6 |
| users | | Object | N | User Mapping section | Note 7 |

1 The base DN, least significant RDN first
2 Username and password are for authentication to the LDAP server, if used; if not used, just leave them as empty strings
3 scope is one of 'sub'/'subtree', 'one'/'onelevel' or 'base'/'baseobject'
  - If the LDAP implementation supports 'subordinate' or 'children' (these are synonymous) then these are available as scopes as well
  - The default is 'sub'
4 One of filter or filter\_local is required if the ldap section is present.  If both are present, filter takes precendence.  An
  attribute set to the empty string is equivalent to it being absent.
5 The service name is a string, which should name a group
6 The 'cloud' section implements a callout to a server to fetch a group membership file
7 The 'users' section provides mappings from the username attribute (selected with username\_attribute) to a local user id.

### Bypass

The module provides a feature for bypassing authentication altogether, letting the process fall through to the next PAM
module in the stack (provided the PAM authentication is configured correctly; see the HOWTO for further details.)  A
typical use case is to treat root logins separately.

There are currently two ways of bypassing; one is an LDAP lookup based on the "preauth" query, and the other is a
special 'users' section where the remote username is the magic string `*bypass*`; if the local username is in this
section, the PAM module is bypassed.  See the HOWTO for further details.

### Deprecated?

- Future releases should change the `client_debug` to loglevel.
  - Additionally, adding `debug` to the PAM config should enable debug, as expected for a PAM module
- The metadata file called `project_id` currently has a backwards compatible default of `/mnt/context/openstack/latest/meta_data.json`


## Authorisation

The major refactoring of the module in Sep 2021 preserved (and bugfixed) the existing authorisation functionality.  However, the user should be warned that it is subject to revision, but generally preserving backward functionality if possible.

### Comprehensive Example

As above, user Fred Bloggs logs in with `ssh fred@example.com`.  The host at `example.com` asks Fred to authenticate.  Once successfully, it calls out to IAM to obtain the userinfo structure.  From this it picks the attribute specified with `username_attribute` in the configuration, `preferred_username`, say.  Let's say the value of `preferred_username` of Fred's userinfo structure is `bloggs`.  Additionally, the userinfo structure contains the list of groups "users", "iris" and "cloud".

Throughout the rest of this section, it is assumed that Fred has authenticated successfully to IAM.

If the **cloud** section is configured and `access` is true, a local file configured as the `metadata_file` is read.  This file should contain the structure

```
{"project\_id": "fleeps"}
```

The module adds the string `fleeps` to the endpoint (with a slash) and calls the server (with *no* client authentication) to see what is at the endpoint.  It expects a JSON structure as response, structured like

```
{"groups": ["wop", "fap", "foo", "users"]}
```

If one of these groups matches Fred's groups as returned by the userinfo structure (it does here, "users"), then Fred is considered authorised.  An additional check is made whether the local username plus suffix equals the remote username.  If this check and the cloud group membership both pass, then Fred is considered authorised (the username check would fail in this example, because no suffix can make 'fred' equal to 'bloggs'.)


If the **group** section is configured and `access` is set to true, a check will be made whether the configured value for `service_name` is one of Fred's groups.  Note the service name is single valued.  Additionally, as for the cloud section, the local username plus suffix must equal the remote username.

If Fred's remote username were `fred_fleeps` then it *would* match the local username (`fred`) if the suffix were configured as `_fleeps`.

If Fred is not authorised through  the cloud or group sections, either because the check failed or they were not enabled, then a configured usermap is consulted.  This is written straight into the configuration file (it should probably be in its own file at some point), so would be suitable only for a smallish number of users.

This usermap is in the **users* section which expects a JSON object mapping the *remote* username to an array of permissible local usernames. Thus, the same user could have multiple local logins using this method.  If the local username is found here, Fred is considered authorised.  No suffix is used in this section.

If Fred is not authorised through any of these methods, the module falls back to an LDAP lookup (if the **ldap** section is configured).  The LDAP query takes a configured filter and substitutes the *remote* username for a `%s` part of the filter, and queries a configured attribute.  There is also a filter\_local which, if filter is left out, will substitute the *local* username and run the query with that instead.

If the filter is `(&(objectClass=user)(cn=%s))` then `bloggs` is substituted in our example, and a target attribute (configured with `attr`) is queried from the LDAP server.  For example, if `attr` has the value `uid` then the equivalent of

```
ldapsearch -x -H ldap://host -b base '(&(objectClass=user)(cn=bloggs))' uid
```

If this query is successful, then Fred is again considered authorised.

If, alternatively, filter\_local is used, the local username is substituted, and the query for `attr` is run with the local username, checking whether it has a `uid` attribute.

## SSH Configuration

You MUST edit the configuration before this module will work!

Make sure the module works correctly before changing your SSH config or you may be locked out!  See Testing below.

Edit `/etc/pam.d/sshd` and comment out the other `auth` sections (unless you need MFA or something else).

```
auth required pam_oauth2_device.so
```

Edit `/etc/ssh/sshd_config` and make sure that the following configuration options are set

```
ChallengeResponseAuthentication yes
UsePAM yes
```

```
systemctl restart sshd
```

## Testing the module works

You are advised to do this before making changes to your main SSH config.  There are two tests to do which are recommended to do in the order described here.

### Preparing for the tests

It is recommended that you create a (hard or sym) link to your `sshd` called (for example) `pamsshd`, e.g. `/usr/local/sbin/pamsshd`.  This means you can have a PAM configuration for `pamsshd` which is different from the normal `sshd`.

In this case you can copy `/etc/pam.d/sshd` to `/etc/pam.d/pamsshd` and edit the latter, leaving the former to log you back into the system if something goes wrong.  Also copy `/etc/ssh/sshd_config` to `/etc/ssh/pamsshd_config` so you can edit the configuration for `pamsshd` separately.

Note that testing *requires* that you install the module in the system location and you have the configuration set up in `/etc/pam_oauth2_device/config.json` and `/etc/pam.d/` and `/etc/ssh/`.

### Test 1: pam tester

Follow instructions above, and additionally install `pamtester` (in Rocky, install EPEL (`epel-release`) first)

Run
```
pamtester -v pamtester localusername authenticate
```
and follow the onscreen prompts.  Here, `localusername` refers to your local user name so replace it with whatever your name is.

Note that if testing as an unprivileged user, pamtester must have permissions to read the configuration file.

You can check `/var/log/secure` (Rocky) or `/var/log/auth.log` (Debian/Ubuntu) to find what's wrong if there are errors authenticating.  While the module
uses syslog, syslog is normally set up to log PAM stuff into one of these files.

### Test 2: sshd

While pamtester tests the authenticate section of the PAM configuration, you should try a proper ssh login from another host.  If you created `pamsshd` as above (and copied configuration as described above), start it with

```
/usr/local/sbin/pamsshd -f /etc/ssh/pamsshd_config -p 2222 -d
```
Since it's listening on port 2222, it can run as an unprivileged user, but needs permission to read the configuration file.

This should start `sshd` with the name `pamsshd` listening on port 2222.  Now try to log in from another host (bearing in mind the port should be open for incoming tcp).  On the other host, run `ssh -p 2222 localusername@myhost` where `localusername` is the local user name and `myhost` is the host running `pamsshd`.

Again check the logs as in the previous tests.

## Troubleshooting and Configuring OpenID Providers (OPs) to use with pam_oauth2_device

This section provides some guidance on configuring the client to work with the module and troubleshooting
if it doesn't work.

### Client Registration and Configuration

1. Check that the certificate of the OP's endpoint(s) validate(s) with the CA bundle or CA path
   - Use a browser to save the certificate into a file, `cert.pem`, say
   - Then check with this command:

```
openssl verify -CApath /etc/pki/certs cert.pem
```
or
```
openssl verify -CAfile /etc/pki/ca-bundle.pem cert.pem
```
where we have assumed your paths/bundles are in `/etc/pki` as above.  If the certificate does not validate, consider
installing the root of the validation path into `/etc/pki`, e.g. `/etc/pki/root.pem`.  The easiest way to find the
certificate is to use a browser to save it.

2. Check that the scopes being requested by the PAM module is a subset of those allowed to the client
   - Check that `offline_access` is included (on both ends)
   - Check that the attribute used as `username_attribute` is included (on both ends)
     - E.g. if `username_attribute` has the value `preferred_username`, ensure that the client is allowed
       `preferred_username`


## Notable Versions

- v1.04 fix for QR code from lahirujayathilake (from Escape chars to Unicode)
- v1.03 support for segmented configuration files (as described above)
- v1.00 (commit efd50f6376a65319a2834b957a2cf6bbe6b7aa2d) - first version for full scale DiRAC test