# ProxyChanger

# What is ProxyChanger?
ProxyChanger is a lightweight command-line utility that automatically fetches, tests, and applies HTTP proxies on a rotating schedule.

It's designed to seamlessly integrate with the Fish shell environment variables system, making it easy to maintain anonymity or overcome geo-restrictions.

# Features
- Automatic proxy discovery: Fetches fresh proxy lists from ProxyScrape API
- Proxy validation: Tests each proxy before applying it to ensure functionality
- Timed rotation: Automatically switches proxies at configurable intervals (default: 20 minutes)
- Fish shell integration: Updates Fish configuration for system-wide proxy settings
- Helper script: Provides a convenient Fish script to apply proxy settings in new terminal sessions

# How It Works
The program fetches a list of proxy servers from ProxyScrape

Each proxy is tested for connectivity and reliability

Working proxies are applied to the Fish shell configuration

Proxies are automatically rotated on a schedule

A helper script allows for easy application in new terminal sessions


# Usage
<pre><code># Clone the repository
git clone https://github.com/scriniariii/proxychanger.git
cd proxychanger

# Compile the program
gcc -o proxychanger proxychanger.c -lcurl

# Make executable
chmod +x proxychanger</code></pre>

<pre><code># Run the program
./proxychanger

# In a new terminal, apply the current proxy settings
fish ./apply_proxy.fish</code></pre>

# Requirements
- C compiler (gcc/clang)
- libcurl development library
- Fish shell
- Linux/Unix environment
