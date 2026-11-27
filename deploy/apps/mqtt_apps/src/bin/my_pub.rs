// MQTT video publisher, counterpart of apps/zenoh_apps my_pub: streams an MPEG-TS file in chunks on
// demo/example/video/<id>, paced to the video's bitrate, looping forever. MQTT 5, QoS 0.
// Each chunk carries a user property "att" = "<seq> <send time ns>" (like the Zenoh attachment).
// If the client's send queue is full (connection congested) the chunk is dropped, like Zenoh's default put.
//
// Args: -h <broker ip> (default 127.0.0.1)  -p <port> (default 1883)
//       -f <file.ts> (default: the tree-and-sun video)  -b <bits per second> (default 3200000)
#[path = "../common.rs"]
mod common;

use rumqttc::v5::mqttbytes::v5::{Packet, PublishProperties};
use rumqttc::v5::mqttbytes::QoS;
use rumqttc::v5::{AsyncClient, Event, MqttOptions};
use std::time::Duration;

const KEY: &str = "demo/example/video";
const CHUNK: usize = 188 * 348; // whole MPEG-TS packets (188 B), ~64 KiB
const QUEUE: usize = 10; // chunks buffered in the client before dropping

#[tokio::main]
async fn main() {
    let broker = common::arg("-h", "127.0.0.1");
    let port: u16 = common::arg("-p", "1883").parse().unwrap();
    let file = common::arg("-f", "/apps/video/video_tree_and_sun.ts");
    let bitrate: f64 = common::arg("-b", "3200000").parse().unwrap();
    let video = std::fs::read(&file).expect("cannot read video file");
    let pause = Duration::from_secs_f64(CHUNK as f64 * 8.0 / bitrate); // real-time pacing

    let id = common::random_id();
    let mut opts = MqttOptions::new(&id, &broker, port);
    opts.set_max_packet_size(Some(common::MAX_PACKET));
    let (client, mut eventloop) = AsyncClient::new(opts, QUEUE);

    // Drive the connection; reconnects on error. Signal the first ConnAck.
    let (connected_tx, connected_rx) = tokio::sync::oneshot::channel();
    tokio::spawn(async move {
        let mut connected_tx = Some(connected_tx);
        loop {
            match eventloop.poll().await {
                Ok(Event::Incoming(Packet::ConnAck(_))) => {
                    if let Some(tx) = connected_tx.take() {
                        let _ = tx.send(());
                    }
                }
                Ok(_) => {}
                Err(e) => {
                    eprintln!("{} connection error: {e}", common::now_ns());
                    tokio::time::sleep(Duration::from_secs(1)).await;
                }
            }
        }
    });
    connected_rx.await.unwrap();

    let topic = format!("{KEY}/{id}"); // own sub-topic per publisher
    println!("{} session opened, id {id}", common::now_ns());
    println!("{} streaming {file} ({} bytes) at {bitrate} bit/s on {topic}", common::now_ns(), video.len());

    let mut seq = 0u64;
    loop {
        for chunk in video.chunks(CHUNK) {
            let sent = common::now_ns();
            let props = PublishProperties {
                user_properties: vec![("att".into(), format!("{seq} {sent}"))],
                ..Default::default()
            };
            match client.try_publish_with_properties(&topic, QoS::AtMostOnce, false, chunk.to_vec(), props) {
                Ok(()) => println!("{sent} sent {seq} {}", chunk.len()),
                Err(_) => println!("{sent} drop {seq} {}", chunk.len()),
            }
            seq += 1;
            tokio::time::sleep(pause).await;
        }
    }
}
