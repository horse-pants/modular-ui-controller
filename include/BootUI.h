#pragma once

#include <lvgl.h>

/**
 * @brief Boot UI class for displaying boot messages and status during startup
 * 
 * This class manages the boot screen display, providing a clean interface
 * for showing startup messages and status information to the user.
 */
class BootUI {
private:
    lv_obj_t* textArea_;        ///< Text area for boot messages
    lv_obj_t* titleLabel_;      ///< Title label
    bool initialized_;          ///< Initialization status
    
    /// Apply synth theme styling to the screen background
    void applyScreenTheme();
    
    /// Create and style the title label
    void createTitle();
    
    /// Create and style the text area for boot messages
    void createTextArea();

public:
    /**
     * @brief Constructor
     */
    BootUI();
    
    /**
     * @brief Destructor - cleans up LVGL objects
     */
    ~BootUI();
    
    // Delete copy constructor and copy assignment operator
    BootUI(const BootUI&) = delete;
    BootUI& operator=(const BootUI&) = delete;
    
    // Allow move constructor and move assignment operator
    BootUI(BootUI&& other) noexcept;
    BootUI& operator=(BootUI&& other) noexcept;
    
    /**
     * @brief Initialize the boot UI screen
     * 
     * Sets up the boot screen with title and text area for messages.
     * Must be called before using addText().
     * 
     * @return true if initialization successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Add text to the boot message area
     * 
     * Appends the given text to the boot message display.
     * The UI must be initialized before calling this method.
     * 
     * @param text The text to add (null-terminated string)
     */
    void addText(const char* text);
    
    /**
     * @brief Clear all text from the boot message area
     */
    void clearText();
    
    /**
     * @brief Check if the boot UI is initialized
     * 
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief Clean up and destroy the boot UI
     * 
     * Removes all LVGL objects and resets the screen.
     * Called automatically by destructor.
     */
    void cleanup();
};