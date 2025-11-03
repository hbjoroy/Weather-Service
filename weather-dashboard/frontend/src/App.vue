<template>
  <div id="app" class="min-h-screen bg-gradient-to-br from-blue-50 to-indigo-100">
    <!-- Header -->
    <header class="bg-white shadow-sm border-b border-gray-200">
      <div class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div class="flex justify-between items-center py-4">
          <div class="flex items-center space-x-2 sm:space-x-4 min-w-0">
            <div class="w-8 h-8 bg-gradient-to-r from-blue-500 to-indigo-600 rounded-lg flex items-center justify-center flex-shrink-0">
              <CloudSun class="w-5 h-5 text-white" />
            </div>
            <h1 class="text-lg sm:text-2xl font-bold text-gray-900 truncate">Weather Dashboard</h1>
          </div>
          
          <!-- User Profile -->
          <div class="flex items-center space-x-2 sm:space-x-4 flex-shrink-0">
            <div class="text-right hidden sm:block">
              <p class="text-sm font-medium text-gray-900">{{ profile.name }}</p>
              <p class="text-xs text-gray-500">{{ profile.tempUnit === 'celsius' ? 'Celsius' : 'Fahrenheit' }}</p>
            </div>
            <button 
              @click="isProfileModalVisible = true"
              class="p-2 rounded-full bg-gray-100 hover:bg-gray-200 transition-colors"
              :title="profile.name"
            >
              <Settings class="w-5 h-5 text-gray-600" />
            </button>
          </div>
        </div>
      </div>
    </header>

    <!-- Main Content -->
    <main class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      <!-- Location Input -->
      <div class="mb-8">
        <div class="max-w-md mx-auto">
          <label for="location" class="block text-sm font-medium text-gray-700 mb-2">
            Location
          </label>
          <div class="relative">
            <input
              id="location"
              v-model="userInputLocation"
              type="text"
              placeholder="Enter city name or coordinates, then click search..."
              class="w-full px-4 py-3 pl-10 pr-4 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
            <MapPin class="absolute left-3 top-3.5 w-5 h-5 text-gray-400" />
            <button
              @click="triggerWeatherDataRefresh"
              :disabled="isLoadingWeatherData || !userInputLocation.trim()"
              class="absolute right-2 top-2 px-3 py-1.5 bg-blue-500 text-white rounded-md hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
            >
              <Search class="w-4 h-4" />
            </button>
          </div>
        </div>
      </div>

      <!-- Loading State -->
      <div v-if="isLoadingWeatherData" class="text-center py-12">
        <div class="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500"></div>
        <p class="mt-2 text-gray-600">Loading weather data...</p>
      </div>

      <!-- Error State -->
      <div v-else-if="weatherLoadError" class="text-center py-12">
        <AlertCircle class="w-12 h-12 text-red-500 mx-auto mb-4" />
        <h3 class="text-lg font-semibold text-gray-900 mb-2">Error Loading Weather Data</h3>
        <p class="text-gray-600 mb-4">{{ weatherLoadError }}</p>
        <button 
          @click="triggerWeatherDataRefresh"
          class="px-4 py-2 bg-blue-500 text-white rounded-lg hover:bg-blue-600 transition-colors"
        >
          Try Again
        </button>
      </div>

      <!-- Weather Content -->
      <div v-else-if="currentWeatherData" class="space-y-8">
        <!-- Tabs -->
        <div class="flex justify-center">
          <div class="bg-white rounded-lg p-1 shadow-sm border border-gray-200">
            <button
              @click="selectedTab = 'current'"
              :class="[
                'px-6 py-2 rounded-md font-medium transition-colors',
                selectedTab === 'current' 
                  ? 'bg-blue-500 text-white shadow-sm' 
                  : 'text-gray-600 hover:text-gray-900'
              ]"
            >
              Current Weather
            </button>
            <button
              @click="selectedTab = 'forecast'"
              :class="[
                'px-6 py-2 rounded-md font-medium transition-colors',
                selectedTab === 'forecast' 
                  ? 'bg-blue-500 text-white shadow-sm' 
                  : 'text-gray-600 hover:text-gray-900'
              ]"
            >
              Forecast
            </button>
          </div>
        </div>

        <!-- Current Weather Tab -->
        <CurrentWeatherCard 
          v-if="selectedTab === 'current'"
          :weather="currentWeatherData"
          :temperature-unit="profile.tempUnit"
          :wind-unit="profile.windUnit"
        />

        <!-- Forecast Tab -->
        <ForecastSection
          v-show="selectedTab === 'forecast'"
          :location="activeSearchLocation"
          :temperature-unit="profile.tempUnit"
          :wind-unit="profile.windUnit"
          :forecast-params="forecastRequestParams"
          :refresh-trigger="forecastRefreshCounter"
          @update-params="updateForecastConfiguration"
        />
      </div>
    </main>

    <!-- Profile Modal -->
    <ProfileModal
      v-if="isProfileModalVisible"
      :profile="profile"
      @update="saveUpdatedProfile"
      @login="handleUserLogin"
      @logout="handleUserLogout"
      @close="isProfileModalVisible = false"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, computed, watch, nextTick } from 'vue'
import { CloudSun, Settings, MapPin, Search, AlertCircle } from 'lucide-vue-next'
import CurrentWeatherCard from '@/components/CurrentWeatherCard.vue'
import ForecastSection from '@/components/ForecastSection.vue'
import ProfileModal from '@/components/ProfileModal.vue'
import api from '@/api/client'
import type { UserProfile, WeatherResponse, ForecastRequest } from '@/types/weather'

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

// User profile and preferences
const profile = ref<UserProfile>({
  userId: '',
  name: 'Guest',
  isAuthenticated: false,
  tempUnit: 'celsius',
  windUnit: 'ms',
  defaultLocation: 'Paros'
})

// Location state
const userInputLocation = ref('')      // What user types in input field
const activeSearchLocation = ref('')   // Current location being searched/displayed

// Weather data
const currentWeatherData = ref<WeatherResponse | null>(null)

// UI state
const selectedTab = ref<'current' | 'forecast'>('current')
const isProfileModalVisible = ref(false)
const isLoadingWeatherData = ref(false)
const weatherLoadError = ref('')
const forecastRefreshCounter = ref(0)

// Forecast configuration
const forecastRequestParams = ref<Omit<ForecastRequest, 'location'>>({
  days: 5,
  include_aqi: false,
  include_alerts: false,
  include_hourly: false
})

// ============================================================================
// PROFILE MANAGEMENT
// ============================================================================

const fetchUserProfile = async (): Promise<void> => {
  try {
    const fetchedProfile = await api.getProfile()
    profile.value = fetchedProfile
    userInputLocation.value = fetchedProfile.defaultLocation
  } catch (err) {
    console.error('Failed to load profile:', err)
    // Continue with default profile if API fails
  }
}

const saveUpdatedProfile = async (newProfile: UserProfile): Promise<void> => {
  try {
    const updatedProfile = await api.updateProfile(newProfile)
    profile.value = updatedProfile
    isProfileModalVisible.value = false
    
    // Update input field if default location changed
    if (userInputLocation.value !== updatedProfile.defaultLocation) {
      userInputLocation.value = updatedProfile.defaultLocation
    }
  } catch (err) {
    console.error('Failed to update profile:', err)
    weatherLoadError.value = 'Failed to update profile: ' + (err as Error).message
  }
}

const handleUserLogin = async (userId: string, name: string, useLegacy: boolean): Promise<void> => {
  try {
    await api.login({ userId, name }, useLegacy)
    // Reload profile after login
    await fetchUserProfile()
    isProfileModalVisible.value = false
  } catch (err) {
    console.error('Failed to login:', err)
    weatherLoadError.value = 'Login failed: ' + (err as Error).message
  }
}

const handleUserLogout = async (): Promise<void> => {
  try {
    await api.logout()
    // Reload profile to get default profile
    await fetchUserProfile()
    isProfileModalVisible.value = false
  } catch (err) {
    console.error('Failed to logout:', err)
    weatherLoadError.value = 'Logout failed: ' + (err as Error).message
  }
}

// ============================================================================
// WEATHER DATA LOADING
// ============================================================================

const fetchCurrentWeatherForLocation = async (): Promise<void> => {
  if (!activeSearchLocation.value.trim()) return
  
  try {
    currentWeatherData.value = await api.getCurrentWeather(
      activeSearchLocation.value, 
      forecastRequestParams.value.include_aqi
    )
  } catch (err) {
    throw new Error('Failed to load current weather: ' + (err as Error).message)
  }
}

const triggerWeatherDataRefresh = async (): Promise<void> => {
  if (!userInputLocation.value.trim()) return
  
  // Update the active search location - this triggers data loading
  activeSearchLocation.value = userInputLocation.value.trim()
  
  // Trigger forecast refresh after location update
  await nextTick()
  forecastRefreshCounter.value++
}

// ============================================================================
// FORECAST CONFIGURATION
// ============================================================================

const updateForecastConfiguration = (params: Omit<ForecastRequest, 'location'>): void => {
  forecastRequestParams.value = params
}

// ============================================================================
// WATCHERS - Automatic data loading when state changes
// ============================================================================

// Watch for active search location changes and load current weather
watch(activeSearchLocation, async (newLocation) => {
  if (!newLocation.trim()) {
    currentWeatherData.value = null
    return
  }
  
  isLoadingWeatherData.value = true
  weatherLoadError.value = ''
  
  try {
    await fetchCurrentWeatherForLocation()
  } catch (err) {
    weatherLoadError.value = (err as Error).message
    currentWeatherData.value = null
  } finally {
    isLoadingWeatherData.value = false
  }
})

// Watch for tab changes and refresh data if needed
watch(selectedTab, async (newTab: string) => {
  const hasSearchLocation = activeSearchLocation.value.trim()
  const notCurrentlyLoading = !isLoadingWeatherData.value
  
  if (hasSearchLocation && notCurrentlyLoading) {
    if (newTab === 'current' && !currentWeatherData.value) {
      await fetchCurrentWeatherForLocation()
    } else if (newTab === 'forecast') {
      forecastRefreshCounter.value++
    }
  }
})

// ============================================================================
// LIFECYCLE HOOKS
// ============================================================================

onMounted(async () => {
  // Check if we're returning from OIDC callback
  const urlParams = new URLSearchParams(window.location.search)
  if (urlParams.has('error')) {
    const error = urlParams.get('error')
    console.error('Authentication error:', error)
    weatherLoadError.value = 'Authentication failed. Please try again.'
    // Clean URL
    window.history.replaceState({}, document.title, window.location.pathname)
  }
  
  // Fetch user profile (will have session if OIDC succeeded)
  await fetchUserProfile()
  
  // Load initial weather data if we have a default location
  const hasDefaultLocation = userInputLocation.value && 
                             userInputLocation.value === profile.value.defaultLocation
  
  if (hasDefaultLocation) {
    activeSearchLocation.value = userInputLocation.value
  }
})
</script>

<style scoped>
/* Custom styles if needed */
</style>