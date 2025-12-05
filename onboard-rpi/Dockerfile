# Use Debian 12 (Bookworm) as the base image
FROM debian:bookworm

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install web development tools
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    qtbase5-dev \
    qtchooser \
    qttools5-dev-tools \
    qttools5-dev \
    qtcreator \
    libqt5widgets5 \
    libqt5gui5 \
    libqt5core5a \
    build-essential \
    libsqlite3-dev \
    git \
    curl \
    wget \
    vim \
    nano \
    sudo \
    iproute2 \
    can-utils \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/venv \
    && . /opt/venv/bin/activate \
    && pip install --no-cache-dir python-can
# Create a non-root user "racing" with UID/GID 1000
# 1000 is the default UID for the first user on most systems
ARG USERNAME=racing
ARG USER_UID=1000
ARG USER_GID=1000
RUN groupadd -g $USER_GID $USERNAME \
    && useradd -m -u $USER_UID -g $USER_GID -s /bin/bash $USERNAME \
    && usermod -aG sudo $USERNAME

# Create project folder for bind mount and ensure correct ownership
RUN mkdir -p /workspace && chown -R $USERNAME:$USERNAME /workspace
COPY setup.sh /usr/local/bin/setup.sh
RUN chmod +x /usr/local/bin/setup.sh

# Switch to the non-root user
USER $USERNAME

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/usr/local/bin/setup.sh"]
ENV PATH="/opt/venv/bin:$PATH"