#include "gtest/gtest.h"
#include "pam_oauth2_device.hpp"


#define DEVICE_ENDPOINT "http://localhost:8042/devicecode"
#define TOKEN_ENDPOINT "http://localhost:8042/token"
#define USERINFO_ENDPOINT "http://localhost:8042/userinfo"
#define USERNAME_ATTRIBUTE "preferred_username"
#define CLIENT_ID "client_id"
#define CLIENT_SECRET "NDVmODY1ZDczMGIyMTM1MWFlYWM2NmYw"
#define SCOPE "openid profile"
#define USER_CODE "QWERTY"
#define DEVICE_CODE "e1e9b7be-e720-467e-bbe1-5c382356e4a9"
#define ACCESS_TOKEN "ZjBhNTQxYzEzMGQwNWU1OWUxMDhkMTM5"
#define VERIFICATION_URL "http://localhost:8042/oidc/device"

namespace
{

TEST(PamTest, Device)
{
    Config config;
    pam_oauth2_log logger(nullptr, pam_oauth2_log::log_level_t::DEBUG);
    DeviceAuthResponse response = make_authorization_request(config, logger);
    EXPECT_EQ(response.user_code, USER_CODE);
    EXPECT_EQ(response.device_code, DEVICE_CODE);
    EXPECT_EQ(response.verification_uri, VERIFICATION_URL);
    EXPECT_EQ(response.verification_uri_complete,
              std::string(VERIFICATION_URL) + "?user_code=" + DEVICE_CODE);
}

TEST(PamTest, Token)
{
    Config config;
    pam_oauth2_log logger(nullptr, pam_oauth2_log::log_level_t::DEBUG);
    std::string token;
    poll_for_token(config, logger, DEVICE_CODE, TODO, TODO);
    EXPECT_EQ(token, ACCESS_TOKEN);
}

TEST(PamTest, Userinfo)
{
    Config config;
    pam_oauth2_log logger(nullptr, pam_oauth2_log::log_level_t::DEBUG);
    Userinfo userinfo("YzQ4YWIzMzJhZjc5OWFkMzgwNmEwM2M5", "jdoe", "Joe Doe");
    get_userinfo(config, logger, ACCESS_TOKEN);
    EXPECT_EQ(userinfo.sub(), "YzQ4YWIzMzJhZjc5OWFkMzgwNmEwM2M5");
    EXPECT_EQ(userinfo.username(), "jdoe");
    EXPECT_EQ(userinfo.name(), "Joe Doe");
}

} // namespace