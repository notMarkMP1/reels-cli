import logging
import os
from datetime import datetime

def setup_logger(name: str = "reels-cli", log_level: int = logging.INFO) -> logging.Logger:
    """
    Sets up a logger that writes to both file and console.
    
    Args:
        name: Logger name
        log_level: Logging level (default: INFO)
    
    Returns:
        Configured logger instance
    """
    logger = logging.getLogger(name)

    if logger.handlers:
        return logger

    logger.setLevel(log_level)

    log_dir = os.path.join(os.path.dirname(__file__), "logs")
    os.makedirs(log_dir, exist_ok=True)

    log_filename = os.path.join(log_dir, f"reels-cli-{datetime.now().strftime('%Y-%m-%d')}.log")

    detailed_formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(filename)s:%(lineno)d - %(message)s'
    )

    file_handler = logging.FileHandler(log_filename)
    file_handler.setLevel(log_level)
    file_handler.setFormatter(detailed_formatter)

    logger.addHandler(file_handler)

    return logger

logger = setup_logger()
