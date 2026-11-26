#ifndef PAM_OAUTH2_DEVICE_HPP
#define PAM_OAUTH2_DEVICE_HPP

#include <string>
#include "include/pam_oauth2_log.hpp"
#include "include/config.hpp"


/*! @brief userinfo type object (cf RFC 7662)
 */

class Userinfo
{
private:
    std::string sub_,
        username_,
        name_;
    // groups will be sorted alphabetically
    std::vector<std::string> groups_;
public:
    Userinfo(std::string const &sub, std::string const &username, std::string const &name): sub_(sub), username_(username), name_(name) {}

    /*! @brief Add a group to the userinfo groups.
     * Caution: there is no check whether the group is already in the userinfo groups.
     */
    void add_group(std::string const &group);

    /*! @brief Import a vector of group names into the userinfo groups.
     * If there are already groups set, they will be removed (no merge).
     */
    void set_groups(std::vector<std::string> const &groups);

    std::string name() const { return name_; }
    std::string sub() const { return sub_; }
    std::string username() const { return username_; }

    //! Check if a given group is part of the userinfo groups
    bool is_member(std::string const &group) const;

    //! @brief Check whether groups (must be _sorted_; specifed through iterators) have any overlap with the userinfo groups.
    //! False is returned only if they are wholly distinct.
    bool intersects(std::vector<std::string>::const_iterator beg,
		    std::vector<std::string>::const_iterator end) const;
};


// TODO: improve this struct
class DeviceAuthResponse
{
public:
    std::string user_code,
        verification_uri,
        verification_uri_complete,
        device_code;
    std::chrono::seconds interval;  //!< Server-specified duration between polling for response
	std::chrono::time_point<std::chrono::system_clock> expires_at; //!< Time to give up

    /*! \brief Generate a prompt for the user to authenticate.
     * The prompt is based on the current values in the structure.
     *
     * \param qr_ecc The error correction level (-1=no QR, 0=low (default), 1=medium, 2=high)
     * \return The prompt, as a std::string
     */
    std::string get_prompt(int qr_ecc);
};

/*! \brief Create the OAuth2 authorisation request, send it to the server and collect the response
 *
 * \param config The configuration class
 * \param logger A logger
 * \return response The response from the server to the request
 */

DeviceAuthResponse make_authorization_request(Config const &config,
                                pam_oauth2_log &logger);


/*! \brief Poll the server to ask if a token is available
 *
 * \param config The ubiquitous config structure
 * \param logger A reference to a logger
 * \param device_code The device code obtained from a successful DeviceAuthResponse
 * \param interval A chrono duration for the interval between polling
 * \param timeout An absolute time to give up (deliberately a different type from the interval)
 * \return the token
 */

std::string poll_for_token(Config const &config,
                           pam_oauth2_log& logger,
                           std::string const& device_code, std::chrono::seconds interval, std::chrono::time_point<std::chrono::system_clock>
                           timeout);

/*! \brief Call the OpenID Provider to obtain the UserInfo data for the token
 *
 * \param config The configuration structure
 * \param logger A logging instance
 * \param token The token obtained previously
 */
Userinfo get_userinfo(Config const &config,
                      pam_oauth2_log &logger,
                      std::string const &token);

#endif // PAM_OAUTH2_DEVICE_HPP
