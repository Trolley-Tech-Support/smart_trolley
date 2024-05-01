# Smart Trolley with ESP32 and Automatic Payment

<img width="691" alt="Screenshot 2024-03-06 173141" src="https://github.com/Trolley-Tech-Support/smart_trolley/assets/22334352/979d04dc-1910-4649-b0fc-6a81b47fd7db">


### Introduction
This project presents a smart trolley system designed to streamline the shopping experience by leveraging IoT technology. 
Traditional checkout processes can be time-consuming and cumbersome, especially during peak hours. 
Our solution aims to revolutionize the shopping experience by integrating product identification, price display, and automated payment directly within the shopping trolley. 
By utilizing an ESP32 microcontroller and various hardware components, this system offers a user-friendly interface, real-time product information display, and a secure exit mechanism.

### Features
Barcode scanning and product identification using an ESP32 microcontroller and camera module.
Weight-based product monitoring for accurate inventory management.
User interaction through a touch panel interface for adding/removing items and initiating payment.
Real-time display of scanned items, quantities, prices, and total cost on an LCD screen.
Secure exit mechanism controlled by a servo motor, ensuring the door unlocks only after successful payment verification.
Integration with cloud-based database and payment systems for scalability, security, and data management.
Modular design for easy development, maintenance, and potential future upgrades.

### System Architecture

![iot cloud](https://github.com/Trolley-Tech-Support/smart_trolley/assets/22334352/86b0b632-61db-49ab-a986-95c7c3f5c5c0)


The smart trolley system consists of various hardware components connected to the ESP32 microcontroller. 
Communication with cloud-based services, including the database and payment system, is facilitated through APIs. 
The system architecture ensures efficient data flow and secure transactions, enhancing the overall reliability and scalability of the solution.

### Setup Instructions

- Install the ESP-IDF extension for Visual Studio Code for code development.
- Set up the ESP32 microcontroller development environment using the ESP-IDF shell environment.
- Clone this repository to your local machine.
- Flash the code onto the ESP32 board using the ESP-IDF shell environment.
- Ensure all hardware components are properly connected and configured according to the provided schematics.
- Configure the cloud-based database and payment system APIs as per the provided documentation.
- Test the functionality of the smart trolley system by scanning barcodes, adding/removing items, initiating payments, and verifying the secure exit mechanism.

### Contribution Guidelines
We welcome contributions from the developer community to improve and enhance the functionality of the smart trolley system.
If you encounter any issues or have suggestions for improvements, please open an issue on GitHub.
Pull requests are encouraged for bug fixes, feature enhancements, and documentation updates.

### Acknowledgements
Special thanks to the project team members and our esteemed Professor for their invaluable contributions and support throughout the development process.
We acknowledge the research studies referenced in this project for their insights and contributions to the field of smart trolley systems.

### License
This project is licensed under the MIT License. Feel free to use, modify, and distribute the code for both commercial and non-commercial purposes.





