// MQTT video subscriber, counterpart of apps/zenoh_apps my_sub: appends every received chunk to an
// output file (playable live, it's MPEG-TS) and logs
// "<receive ns> recv <seq> <send ns> <latency ms> <bytes> <last 3 chars of publisher id>" per chunk.
//
// Args: -h <broker ip> (default 127.0.0.1)  -p <port> (default 1883)
//       -o <output file> (default /out/received.ts; the launcher passes /out/<container>_video.ts)
#[path = "../common.rs"]
mod common;

use rumqttc::v5::mqttbytes::v5::Packet;
use rumqttc::v5::mqttbytes::QoS;
use rumqttc::v5::{AsyncClient, Event, MqttOptions};
use std::io::Write;
use std::time::Duration;

const KEY: &str = "demo/example/video";

#[tokio::main]
async fn main() {
    let broker = common::arg("-h", "127.0.0.1");
    let port: u16 = common::arg("-p", "1883").parse().unwrap();
    let out_path = common::arg("-o", "/out/received.ts");
    let mut out = std::fs::File::create(&out_path).expect("cannot create output file");

    let id = common::random_id();
    let mut opts = MqttOptions::new(&id, &broker, port);
    opts.set_max_packet_size(Some(common::MAX_PACKET));
    let (client, mut eventloop) = AsyncClient::new(opts, 10);
    let filter = format!("{KEY}/+"); // all publishers

    loop {
        match eventloop.poll().await {
            Ok(Event::Incoming(Packet::ConnAck(_))) => {
                // (Re)subscribe on every connect: a clean session forgets subscriptions
                println!("{} session opened, id {id}", common::now_ns());
                println!("{} writing {filter} to {out_path}", common::now_ns());
                client.subscribe(&filter, QoS::AtMostOnce).await.unwrap();
            }
            Ok(Event::Incoming(Packet::Publish(p))) => {
                let received = common::now_ns();
                out.write_all(&p.payload).unwrap();
                out.flush().unwrap();

                // user property "att" = "<seq> <send ns>" from my_pub
                let att = p.properties.iter().flat_map(|pr| pr.user_properties.iter())
                    .find(|(k, _)| k == "att").map(|(_, v)| v.as_str()).unwrap_or("");
                let mut it = att.split_whitespace();
                let (seq, sent) = (it.next().unwrap_or("?"), it.next().unwrap_or("0"));
                let latency_ms = (received as f64 - sent.parse::<f64>().unwrap_or(0.0)) / 1e6;
                let topic = String::from_utf8_lossy(&p.topic);
                println!("{received} recv {seq} {sent} {latency_ms:.3} {} {}", p.payload.len(),
                         &topic[topic.len().saturating_sub(3)..]); // last 3 chars of publisher id
            }
            Ok(_) => {}
            Err(e) => {
                eprintln!("{} connection error: {e}", common::now_ns());
                tokio::time::sleep(Duration::from_secs(1)).await;
            }
        }
    }
}
