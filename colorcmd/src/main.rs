use std::fs;
use std::env;
use std::net::UdpSocket;
use network_interface::{NetworkInterface, NetworkInterfaceConfig, V4IfAddr, Addr};
use serde::{Serialize, Deserialize};

trait SerAble {
    fn ser(&self, v: &mut Vec<u8>);
}

#[derive(Deserialize, Serialize)]
struct Color {
    g: u8,
    r: u8,
    b: u8,
}

impl SerAble for Color {
    fn ser(&self, v: &mut Vec<u8>) {
        v.push(self.g);
        v.push(self.r);
        v.push(self.b);
    }
}

#[derive(Deserialize, Serialize)]
struct Palette {
    ranges: Vec<(Color, Color)>,
}

impl SerAble for Palette {
    fn ser(&self, v: &mut Vec<u8>) {
        let count = self.ranges.len() as u16;
        v.extend_from_slice(&count.to_le_bytes());

        for rg in &self.ranges {
            rg.0.ser(v);
            rg.1.ser(v);
        }
    }
}

#[derive(Deserialize, Serialize)]
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

#[derive(Deserialize, Serialize)]
struct Gradient {
    pts: Vec<GradPoint>,
}

impl SerAble for Gradient {
    fn ser(&self, v: &mut Vec<u8>) {
        let count = self.pts.len() as u16;
        v.extend_from_slice(&count.to_le_bytes());

        for pt in &self.pts {
            pt.ser(v);
        }
    }
}

#[derive(Deserialize, Serialize)]
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

#[derive(Deserialize, Serialize)]
struct AniFrame {
    duration: u16,
    blend: AniBlend,
    grad: Gradient,
}

impl SerAble for AniFrame {
    fn ser(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self.duration.to_le_bytes());
        v.push(self.blend.as_num());
        self.grad.ser(v)
    }
}

#[derive(Deserialize, Serialize)]
struct AniGradient {
    frames: Vec<AniFrame>,
}

impl SerAble for AniGradient {
    fn ser(&self, v: &mut Vec<u8>) {
        let count = self.frames.len() as u16;
        v.extend_from_slice(&count.to_le_bytes());

        for f in &self.frames {
            f.ser(v);
        }
    }
}

#[derive(Deserialize, Serialize)]
struct RandGradient {
    maxpoints: u8,
    minpoints: u8,
    maxduration: u16,
    minduration: u16,
    colors: Palette,
}

impl SerAble for RandGradient {
    fn ser(&self, v: &mut Vec<u8>) {
        v.push(self.minpoints);
        v.push(self.maxpoints);
        v.extend_from_slice(&self.minduration.to_le_bytes());
        v.extend_from_slice(&self.maxduration.to_le_bytes());
        self.colors.ser(v);
    }
}


#[derive(Deserialize, Serialize)]
struct Popping {
    fadetime: u16,
    maxtillspot: u16,
    mintillspot: u16,
    maxgrowspot: u16,
    mingrowspot: u16,
    maxsize: u16,
    minsize: u16,
    spottypes: u8,
    bg: Color,
    colors: Palette,
}

impl SerAble for Popping {
    fn ser(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self.fadetime.to_le_bytes());
        v.extend_from_slice(&self.mintillspot.to_le_bytes());
        v.extend_from_slice(&self.maxtillspot.to_le_bytes());
        v.extend_from_slice(&self.mingrowspot.to_le_bytes());
        v.extend_from_slice(&self.maxgrowspot.to_le_bytes());
        v.extend_from_slice(&self.minsize.to_le_bytes());
        v.extend_from_slice(&self.maxsize.to_le_bytes());
        v.push(self.spottypes);
        self.bg.ser(v);
        self.colors.ser(v);
    }
}

#[derive(Deserialize, Serialize)]
enum PatternType {
    Grad(Gradient),
    AniGrad(AniGradient),
    RandGrad(RandGradient),
    Popping(Popping),
}

impl PatternType {
    fn as_num(&self) -> u8 {
        match self {
            PatternType::Grad(_) => 1,
            PatternType::AniGrad(_) => 2,
            PatternType::RandGrad(_) => 3,
            PatternType::Popping(_) => 4,
        }
    }
}

impl SerAble for PatternType {
    fn ser(&self, v: &mut Vec<u8>) {
        // contents, not the #
        match self {
            PatternType::Grad(gr) => gr.ser(v),
            PatternType::AniGrad(ag) => ag.ser(v),
            PatternType::RandGrad(rg) => rg.ser(v),
            PatternType::Popping(pp) => pp.ser(v),
        };
    }
}

#[derive(Deserialize, Serialize)]
struct Pattern {
    timeout: u16,
    pat: PatternType,
}

impl Pattern {
    fn serialize(self) -> Vec<u8> {
        let mut v: Vec<u8> = Vec::new();

        self.ser(&mut v);

        v
    }
}

impl SerAble for Pattern {
    fn ser(&self, v: &mut Vec<u8>) {
        v.push(self.pat.as_num());
        v.extend_from_slice(&self.timeout.to_le_bytes());
        self.pat.ser(v);
    }
}


fn send_pattern(pat: Pattern) {

    let buf = pat.serialize();

    // we need to bind to the right interface, or the multicast packet will go out the wrong hole
    // so let's just try them all
    for interface in NetworkInterface::show().unwrap() {
        if let Some(Addr::V4(V4IfAddr{ip: theip, ..})) = interface.addr {
            if !theip.is_loopback() {
                let socket = UdpSocket::bind((theip, 0)).unwrap();
                let amt = socket.send_to(buf.as_slice(), "239.3.6.9:3690").unwrap();
            
                if amt != buf.len() {
                    println!("Warning: Did not send the full packet on interface at {:?}", theip);
                }
            }
        }
    }

}

fn get_test_pat() -> Pattern {
    // a test gradient
    Pattern {
        timeout: 5,
        pat: PatternType::AniGrad(
            AniGradient {
                frames: vec![
                    AniFrame {
                        duration: 30,
                        blend: AniBlend::Linear,
                        grad: Gradient {
                            pts: vec![
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
                            pts: vec![
                                GradPoint { n: 0, c: Color {g: 0x5, r: 0x5, b: 0x5}},
                                GradPoint { n: 100, c: Color {g: 0x20, r: 0, b: 0x10}},
                            ],
                        },
                    },
                    AniFrame {
                        duration: 5,
                        blend: AniBlend::Linear,
                        grad: Gradient {
                            pts: vec![
                                GradPoint { n: 0, c: Color {g: 0x5, r: 0x20, b: 0x10}},
                                GradPoint { n: 100, c: Color {g: 0x5, r: 0x5, b: 0x5}},
                            ],
                        },
                    },
                ],
            }
        ),
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();

    let input_file: String = fs::read_to_string(&args[1]).unwrap();

    let pat: Pattern = serde_json::from_str(&input_file).expect("Invalid json");

    send_pattern(pat);

    println!("Done");
}
