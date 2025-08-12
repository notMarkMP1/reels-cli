import socket
from logger_config import logger

class UDSClient:
    def __init__(self, socket_path: str = "/tmp/uds_socket"):
        self.socket_path = socket_path
        self.client_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            logger.info("Python client attempting to connect to C server...")
            self.client_socket.connect(self.socket_path)
            logger.info(f"Connected to server at {self.socket_path}")
        except (ConnectionRefusedError, FileNotFoundError) as e:
            logger.error(f"Error: {e}")
            logger.error("Make sure the C server is running first!")
            raise
    
    def __del__(self):
        self.client_socket.close()
        logger.info("Client socket closed")

    def send_message(self, message: str):
        """
        Sends a message to the server.
        :param message: The message to send.
        """
        self.client_socket.send(message.encode('utf-8'))
        logger.debug(f"Sent: {message}")
    
    def receive_response(self):
        """
        Receives a response from the server.
        :return: The response message.
        """
        response = self.client_socket.recv(1024)
        if not response:
            return None
        logger.debug(f"Received: {response.decode('utf-8')}")
        return response.decode('utf-8')