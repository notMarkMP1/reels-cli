from api import InstagramClient
from uds_client import UDSClient
from logger_config import logger
import time

SOCKET_PATH = "/tmp/uds_socket"

USERNAME = "your_instagram_username"
PASSWORD = "your_instagram_password"

last_pk = None

if __name__ == "__main__":
    insta_client = InstagramClient(USERNAME, PASSWORD)
    insta_client.login()
    uds_client = UDSClient(SOCKET_PATH)
    while True:
        try:
            response = uds_client.receive_response()
            if response is not None:
                match response:
                    case "exit":
                        break
                    case "fetch":
                        reels = insta_client.fetch_reels(5)
                        str_reels = [str(reel) for reel in reels]
                        for reel in str_reels:
                            uds_client.send_message(reel)
                            time.sleep(0.1)

        except Exception as e:
            logger.error(f"An error occurred: {e}")

