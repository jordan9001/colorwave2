use std::net::UdpSocket;
use network_interface::{NetworkInterface, NetworkInterfaceConfig, V4IfAddr, Addr};

struct Color {
    g: u8,
    r: u8,
    b: u8,
}

struct GradPoint {
    n: u16,
    c: Color,
}

struct Gradient<'a> {
    pts: &'a [GradPoint],
}

impl Gradient<'_> {
    fn ser(self, timeout: u16) -> Vec<u8> {
        // serialize the gradient into a pattern_gradient for the esp32
        let mut v = Vec::new();

        v.push(1); // PATTERN_TYPE_GRADIENT
        v.extend_from_slice(&timeout.to_le_bytes());

        let count = self.pts.len() as u16;
        v.extend_from_slice(&count.to_le_bytes());

        for pt in self.pts {
            v.extend_from_slice(&pt.n.to_le_bytes());
            v.push(pt.c.g);
            v.push(pt.c.r);
            v.push(pt.c.b);
        }

        v
    }
}

fn main() {

    // a test gradient
    let g = Gradient {
        pts: &[
            GradPoint { n: 0, c: Color {g: 0, r: 0x30, b: 0}},
            GradPoint { n: 54, c: Color {g: 0x18, r: 0, b: 0x18}},
            GradPoint { n: 109, c: Color {g: 0, r: 0, b: 0x30}},
        ]
    };

    let gbuf = g.ser(0);

    // we need to bind to the right interface, or the multicast packet will go out the wrong hole
    // so let's just try them all
    for interface in NetworkInterface::show().unwrap() {
        if let Some(Addr::V4(V4IfAddr{ip: theip, ..})) = interface.addr {
            if !theip.is_loopback() {
                let socket = UdpSocket::bind((theip, 0)).unwrap();
                let amt = socket.send_to(gbuf.as_slice(), "239.3.6.9:3690").unwrap();
            
                if amt != gbuf.len() {
                    println!("Warning: Did not send the full packet on interface at {:?}", theip);
                }
            }
        }
    }

    println!("Done");
}
