// Video subscriber: appends every received chunk to an output file (playable live, it's MPEG-TS)
// and logs "<receive ns> recv <seq> <send ns> <latency ms> <bytes>" per chunk.
//
// Args: -o <output file> (default /out/received.ts; the launcher passes /out/<container>_video.ts)
//       -l/-e <endpoint> as passed by the launcher
#[path = "../common.rs"]
mod common;

use std::io::Write;

const KEY: &str = "demo/example/video";

#[tokio::main]
async fn main() {
    let out_path = common::arg("-o", "/out/received.ts");
    let mut out = std::fs::File::create(&out_path).expect("cannot create output file");

    let session = zenoh::open(common::config_from_args()).await.unwrap();
    println!("{} session opened, zid {}", common::now_ns(), session.zid());
    println!("{} writing {KEY} to {out_path}", common::now_ns());

    let subscriber = session.declare_subscriber(format!("{KEY}/*")).await.unwrap(); // all publishers

    while let Ok(sample) = subscriber.recv_async().await {
        let received = common::now_ns();
        let data = sample.payload().to_bytes();
        out.write_all(&data).unwrap();
        out.flush().unwrap();

        // attachment = "<seq> <send ns>" from my_pub
        let att = sample.attachment().and_then(|a| a.try_to_string().ok()).unwrap_or_default();
        let mut it = att.split_whitespace();
        let (seq, sent) = (it.next().unwrap_or("?"), it.next().unwrap_or("0"));
        let latency_ms = (received as f64 - sent.parse::<f64>().unwrap_or(0.0)) / 1e6;
        println!("{received} recv {seq} {sent} {latency_ms:.3} {} {}", data.len(), &sample.key_expr().as_str()[sample.key_expr().as_str().len() - 3..]); // last 3 chars of publisher zid
    }
}
