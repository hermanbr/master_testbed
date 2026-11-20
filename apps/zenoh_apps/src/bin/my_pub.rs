// Video publisher: streams an MPEG-TS file in chunks on KEY, paced to the video's bitrate, looping forever.
// Each chunk carries an attachment "<seq> <send time ns>" so subscribers can compute latency and loss.
//
// Args: -f <file.ts> (default: the tree-and-sun video)  -b <bits per second> (default 3200000)
//       -l/-e <endpoint> as passed by the launcher
#[path = "../common.rs"]
mod common;

use std::time::Duration;

const KEY: &str = "demo/example/video";
const CHUNK: usize = 188 * 348; // whole MPEG-TS packets (188 B), ~64 KiB

#[tokio::main]
async fn main() {
    let file = common::arg("-f", "/apps/zenoh_apps/video_tree_and_sun.ts");
    let bitrate: f64 = common::arg("-b", "3200000").parse().unwrap();
    let video = std::fs::read(&file).expect("cannot read video file");
    let pause = Duration::from_secs_f64(CHUNK as f64 * 8.0 / bitrate); // real-time pacing

    let session = zenoh::open(common::config_from_args()).await.unwrap();
    println!("{} session opened, zid {}", common::now_ns(), session.zid());
    println!("{} streaming {file} ({} bytes) at {bitrate} bit/s on {KEY}", common::now_ns(), video.len());

    let publisher = session.declare_publisher(format!("{KEY}/{}", session.zid())).await.unwrap(); // own sub-key per publisher (zid is random)

    let mut seq = 0u64;
    loop {
        for chunk in video.chunks(CHUNK) {
            let sent = common::now_ns();
            publisher
                .put(chunk.to_vec())
                .attachment(format!("{seq} {sent}"))
                .await
                .unwrap();
            println!("{sent} sent {seq} {}", chunk.len());
            seq += 1;
            tokio::time::sleep(pause).await;
        }
    }
}
