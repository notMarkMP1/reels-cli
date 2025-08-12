from instagrapi import Client
from instagrapi.exceptions import LoginRequired
from logger_config import logger
import os

class InstagramClient:
    def __init__(self, username: str, password: str):
        self.username = username
        self.password = password
        self.client = Client()

    def login(self) -> None:
        """
        Attempts to login to Instagram using either the provided session information
        or the provided username and password.
        """
        if os.path.exists(os.path.join(os.getcwd(), "session.json")):
            session = self.client.load_settings(os.path.join(os.getcwd(), "session.json"))
        else:
            session = None

        login_via_session = False
        login_via_pw = False

        if session:
            try:
                self.client.set_settings(session)
                self.client.login(self.username, self.password)

                # check if session is valid
                try:
                    self.client.get_timeline_feed()
                except LoginRequired:
                    logger.warning("Session is invalid, need to login via username and password")

                    old_session = self.client.get_settings()

                    # use the same device uuids across logins
                    self.client.set_settings({})
                    self.client.set_uuids(old_session["uuids"])

                    self.client.login(self.username, self.password)
                login_via_session = True
            except Exception as e:
                logger.error("Couldn't login user using session information: %s" % e)

        if not login_via_session:
            try:
                logger.info("Attempting to login via username and password. username: %s" % self.username)
                if self.client.login(self.username, self.password):
                    login_via_pw = True
            except Exception as e:
                logger.error("Couldn't login user using username and password: %s" % e)

        if not login_via_pw and not login_via_session:
            raise Exception("Couldn't login user with either password or session")
        
        logger.info("Login successful")
        self.client.dump_settings(os.path.join(os.getcwd(), "session.json"))

    def fetch_reels(self, count: int = 10) -> list[str]:
        """
        Fetches reels from the Instagram explore page.
        :param count: Number of reels to fetch.
        :return: List of CDN URLs for the reels.
        """
        reels = self.client.reels_timeline_media("explore_reels", count)
        return [reel.video_url for reel in reels if reel.video_url]
