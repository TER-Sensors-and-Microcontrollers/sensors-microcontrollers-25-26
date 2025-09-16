# Use Debian 12 (Bookworm) as the base image
FROM debian:bookworm

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install web development tools
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    python3-flask \
    python3-flask-cors \
    python3-flask-restful \
    build-essential \
    git \
    curl \
    wget \
    vim \
    nano \
    && rm -rf /var/lib/apt/lists/*


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

# Switch to the non-root user
USER $USERNAME

# Set working directory
WORKDIR /workspace

# Expose port 5000 (default Flask port)
EXPOSE 5000

# Default command
CMD ["/bin/bash"]