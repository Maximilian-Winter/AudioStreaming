#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

using asio::ip::udp;

class MiniMP3UDPServer : public std::enable_shared_from_this<MiniMP3UDPServer> {
public:
    MiniMP3UDPServer(asio::io_context& io_context, short port)
        : socket_(io_context, udp::endpoint(udp::v4(), port)),
          io_context_(io_context),
          running_(false),
          client_connected_(false) {
    }

    bool start(const std::string& input_file) {
        // Open the MP3 file
        std::ifstream file(input_file, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open input file: " << input_file << std::endl;
            return false;
        }

        // Read the entire file into a vector
        mp3_data_ = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Initialize the MP3 decoder
        mp3dec_init(&mp3d_);

        running_ = true;
        decode_thread_ = std::thread([this]() { decode_and_send(); });

        start_receive();
        return true;
    }

    ~MiniMP3UDPServer() {
        running_ = false;
        if (decode_thread_.joinable()) {
            decode_thread_.join();
        }
    }

private:
    void decode_and_send() {
        mp3dec_frame_info_t info;
        std::vector<int16_t> pcm(MINIMP3_MAX_SAMPLES_PER_FRAME);
        size_t offset = 0;
        int samples;

        while (running_ && offset < mp3_data_.size()) {
            samples = mp3dec_decode_frame(&mp3d_, mp3_data_.data() + offset, mp3_data_.size() - offset, pcm.data(), &info);
            if (samples > 0) {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return client_connected_ || !running_; });
                if (!running_) break;

                size_t bytes_to_send = samples * info.channels * sizeof(int16_t);
                asio::post(io_context_, [this, data = pcm, bytes = bytes_to_send]() {
                    socket_.async_send_to(
                        asio::buffer(data.data(), bytes), remote_endpoint_,
                        [this](std::error_code ec, std::size_t bytes_sent) {
                            if (ec) {
                                std::cerr << "Send error: " << ec.message() << std::endl;
                                client_connected_ = false;
                                start_receive();
                            }
                        });
                });

                // Add a small delay to control the streaming rate
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            offset += info.frame_bytes;
        }

        std::cout << "Finished decoding and sending MP3 data." << std::endl;
    }

    void start_receive() {
        socket_.async_receive_from(
            asio::buffer(recv_buffer_), remote_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec) {
                    if (!client_connected_) {
                        std::cout << "Received data from client. Starting stream..." << std::endl;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            client_connected_ = true;
                        }
                        cv_.notify_one();
                    }
                    // Process received data if needed

                    // Set up the next receive operation
                    start_receive();
                } else {
                    std::cerr << "Receive error: " << ec.message() << std::endl;
                    start_receive();
                }
            });
    }

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 1024> recv_buffer_{};
    asio::io_context& io_context_;
    std::atomic<bool> running_;
    std::thread decode_thread_;
    std::atomic<bool> client_connected_;
    std::mutex mutex_;
    std::condition_variable cv_;

    mp3dec_t mp3d_;
    std::vector<uint8_t> mp3_data_;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <input_file>" << std::endl;
        return 1;
    }

    try {
        asio::io_context io_context;
        auto server = std::make_shared<MiniMP3UDPServer>(io_context, std::stoi(argv[1]));

        if (server->start(argv[2])) {
            io_context.run();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}