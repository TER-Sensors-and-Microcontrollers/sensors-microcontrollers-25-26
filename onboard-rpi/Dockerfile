# Use Debian 12 (Bookworm) as the base image
FROM debian:bookworm

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install web development tools
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    python3-pyqt5 \
    python3-pandas \
    python3-numpy  \
    qtbase5-dev \
    qtchooser \
    qttools5-dev-tools \
    qttools5-dev \
    qtcreator \
    libqt5widgets5 \
    libqt5gui5 \
    libqt5core5a \
    python3-pyqt5 \
    python3-pyqt5.qtsvg \
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
    kmod \
    x11-apps \
    libxcb-xinerama0 \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv --system-site-packages /opt/venv \
    && . /opt/venv/bin/activate \
    && pip install --no-cache-dir \
    python-can 
# Create a non-root user "racing" with UID/GID 1000
# 1000 is the default UID for the first user on most systems
ARG USERNAME=racing
ARG USER_UID=1000
ARG USER_GID=1000
RUN groupadd -g $USER_GID $USERNAME \
    && useradd -m -u $USER_UID -g $USER_GID -s /bin/bash $USERNAME \
    && usermod -aG sudo $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Create project folder for bind mount and ensure correct ownership
RUN mkdir -p /workspace && chown -R $USERNAME:$USERNAME /workspace
COPY setup.sh /usr/local/bin/setup.sh
RUN chmod +x /usr/local/bin/setup.sh

# Switch to the non-root user
USER $USERNAME

# Set working directory
WORKDIR /workspace

# Activate virtual environment by default
ENV PATH="/opt/venv/bin:$PATH"
ENV VIRTUAL_ENV="/opt/venv"

# For Qt/GUI applications
ENV QT_X11_NO_MITSHM=1
ENV DISPLAY=:0

# Default command
CMD ["/usr/local/bin/setup.sh"]