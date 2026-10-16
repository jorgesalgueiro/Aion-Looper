fn main() -> Result<(), Box<dyn std::error::Error>> {
    let input = midir::MidiInput::new("Rust-Looper read-only MIDI discovery")?;
    let ports = input.ports();
    println!("Native MIDI inputs: {}", ports.len());
    for port in ports {
        println!("{}: {}", port.id(), input.port_name(&port)?);
    }
    Ok(())
}
