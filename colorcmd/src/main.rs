use std::net::UdpSocket;
use network_interface::{NetworkInterface, NetworkInterfaceConfig, V4IfAddr, Addr};

trait SerAble {
    fn ser(&self, v: &mut Vec<u8>);
}

struct Color {
    g: u8,
    r: u8,
    b: u8,
}

struct Pallet<'a> {
    ranges: &'a [(Color, Color)],
}

impl SerAble for Color {
    fn ser(&self, v: &mut Vec<u8>) {
        v.push(self.g);
        v.push(self.r);
        v.push(self.b);
    }
}

struct GradPoint {
    n: u16,
    c: Color,
}

impl SerAble for GradPoint {
    fn ser(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self.n.to_le_bytes());
        self.c.ser(v);
    }
}

struct Gradient<'a> {
    pts: &'a [GradPoint],
}

impl SerAble for Gradient<'_> {
    fn ser(&self, v: &mut Vec<u8>) {
        let count = self.pts.len() as u16;
        v.extend_from_slice(&count.to_le_bytes());

        for pt in self.pts {
            pt.ser(v);
        }
    }
}

enum AniBlend {
    Hold,
    Linear,
    Dissolve,
    RSlide,
    LSlide,
}

impl AniBlend {
    fn as_num(&self) -> u8 {
        match self {
            AniBlend::Hold => 1,
            AniBlend::Linear => 2,
            AniBlend::Dissolve => 3,
            AniBlend::RSlide => 4,
            AniBlend::LSlide => 5,
        }
    }
}

struct AniFrame<'a> {
    duration: u16,
    blend: AniBlend,
    grad: Gradient<'a>,
}

impl SerAble for AniFrame<'_> {
    fn ser(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self.duration.to_le_bytes());
        v.push(self.blend.as_num());
        self.grad.ser(v)
    }
}

struct AniGradient<'a> {
    frames: &'a [AniFrame<'a>],
}

impl SerAble for AniGradient<'_> {
    fn ser(&self, v: &mut Vec<u8>) {
        let count = self.frames.len() as u16;
        v.extend_from_slice(&count.to_le_bytes());

        for f in self.frames {
            f.ser(v);
        }
    }
}

struct RandGradient<'a> {
    maxpoints: u8,
    minpoints: u8,
    maxduration: u16,
    minduration: u16,
    colors: Palette<'a>,
}


enum PatternType<'a> {
    Grad(Gradient<'a>),
    AniGrad(AniGradient<'a>),
    RandGrad(RandGradient<'a>),
    Popping(),
}

impl PatternType<'_> {
    fn as_num(&self) -> u8 {
        match self {
            PatternType::Grad(_) => 1,
            PatternType::AniGrad(_) => 2,
            PatternType::RandGrad() => 3,
            PatternType::Popping() => 4,
        }
    }
}

impl SerAble for PatternType<'_> {
    fn ser(&self, v: &mut Vec<u8>) {
        // contents, not the #
        match self {
            PatternType::Grad(gr) => gr.ser(v),
            PatternType::AniGrad(ag) => ag.ser(v),
            PatternType::RandGrad() => (),
            PatternType::Popping() => (),
        };
    }
}

struct Pattern<'a> {
    timeout: u16,
    pat: PatternType<'a>,
}

impl Pattern<'_> {
    fn serialize(self) -> Vec<u8> {
        let mut v: Vec<u8> = Vec::new();

        self.ser(&mut v);

        v
    }
}

impl SerAble for Pattern<'_> {
    fn ser(&self, v: &mut Vec<u8>) {
        v.push(self.pat.as_num());
        v.extend_from_slice(&self.timeout.to_le_bytes());
        self.pat.ser(v);
    }
}


fn main() {

    // a test gradient
    let pat = Pattern {
        timeout: 5,
        pat: PatternType::AniGrad(
            AniGradient {
                frames: &[
                    AniFrame {
                        duration: 30,
                        blend: AniBlend::Linear,
                        grad: Gradient {
                            pts: &[
                                GradPoint { n: 0, c: Color {g: 0, r: 0, b: 0x30}},
                                GradPoint { n: 54, c: Color {g: 0, r: 0x20, b: 0x18}},
                                GradPoint { n: 109, c: Color {g: 0, r: 0x3, b: 0}},
                            ],
                        },
                    },
                    AniFrame {
                        duration: 10,
                        blend: AniBlend::Linear,
                        grad: Gradient {
                            pts: &[
                                GradPoint { n: 0, c: Color {g: 0x5, r: 0x5, b: 0x5}},
                                GradPoint { n: 100, c: Color {g: 0x20, r: 0, b: 0x10}},
                            ],
                        },
                    },
                    AniFrame {
                        duration: 5,
                        blend: AniBlend::Linear,
                        grad: Gradient {
                            pts: &[
                                GradPoint { n: 0, c: Color {g: 0x5, r: 0x20, b: 0x10}},
                                GradPoint { n: 100, c: Color {g: 0x5, r: 0x5, b: 0x5}},
                            ],
                        },
                    },
                ],
            }
        ),
    };

    let gbuf = pat.serialize();

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
