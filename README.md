# Socket File Sharing Application

An application to share files between computers via TCP

## Instructions

```bash
# Build the application
chmod +x build.sh
./build.sh

# Production build
./build.sh prod

# To receive file
./bin/share recv [-p PORT]

# To send a file
./bin/share send <RECEIVER IP> <FILEPATH> [-p PORT] 
```

**Note:** (for receiver) add a firewall exception for the PORT you want to use
