cd "$(dirname "$0")/../zenoh" || exit
cross build --profile fast --all-targets --target x86_64-unknown-linux-musl