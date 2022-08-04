from .common import get_version, YtError
from .http_driver import TokenAuth
from .http_helpers import get_token, format_logging_params

import yt.logger as logger
import yt.wrapper.yson as yson

import os
import requests


def get_full_ctl_address(address):
    if not address:
        address = os.getenv("CHYT_CTL_ADDRESS")
    if not address:
        return "https://production.chyt-ctl.in.yandex-team.ru"
    if address.isalnum():
        return "https://{}.chyt-ctl.in.yandex-team.ru".format(address)
    if not address.startswith("http://") and not address.startswith("https://"):
        return "https://" + address
    return address


def get_user_agent():
    user_agent = "Python wrapper " + get_version()
    if "_ARGCOMPLETE" in os.environ:
        user_agent += " [argcomplete mode]"
    return user_agent


def describe_api(address):
    address = get_full_ctl_address(address)

    url = address + "/describe"
    headers = {
        "User-Agent": get_user_agent(),
    }

    logging_params = {
        "headers": headers,
    }
    logger.debug("Perform HTTP GET request %s (%s)", url, format_logging_params(logging_params))

    response = requests.get(address + "/describe")

    logging_params = {
        "headers": dict(response.headers),
        "status_code": response.status_code,
        "body": response.content,
    }
    logger.debug("Response received (%s)", format_logging_params(logging_params))

    if response.status_code != 200:
        raise YtError("bad response from controller service", attributes={
            "status_code": response.status_code,
            "response_body": response.content})

    return yson.loads(response.content)


def make_request(command_name, params, address, cluster_proxy, unparsed=False):
    address = get_full_ctl_address(address)

    url = "{}/{}/{}".format(address, cluster_proxy, command_name)
    data = yson.dumps({"params": params, "unparsed": unparsed})
    auth = TokenAuth(get_token())
    headers = {
        "User-Agent": get_user_agent(),
        "Content-Type": "application/yson"
    }

    test_user = os.getenv("YT_TEST_USER")
    if test_user:
        headers["X-YT-TestUser"] = test_user

    logging_params = {
        "headers": headers,
        "params": params,
    }
    logger.debug("Perform HTTP POST request %s (%s)", url, format_logging_params(logging_params))

    response = requests.post(url, data=data, auth=auth, headers=headers)

    logging_params = {
        "headers": dict(response.headers),
        "status_code": response.status_code,
        "body": response.content,
    }
    logger.debug("Response received (%s)", format_logging_params(logging_params))

    if response.status_code == 403:
        raise YtError("auhtorization failed; check that your yt token is valid", attributes={
            "response_body": response.content})

    if response.status_code not in [200, 400]:
        raise YtError("bad response from controller service", attributes={
            "status_code": response.status_code,
            "response_body": response.content})

    return yson.loads(response.content)