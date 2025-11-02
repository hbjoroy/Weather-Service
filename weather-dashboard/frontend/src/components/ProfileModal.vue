<template>
  <div class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
    <div class="bg-white rounded-lg p-6 w-full max-w-md mx-4">
      <h3 class="text-lg font-semibold mb-4">User Profile</h3>
      
      <!-- Not authenticated - show login -->
      <div v-if="!profile.isAuthenticated && !showLoginForm">
        <p class="text-gray-600 mb-4">You are using the default guest profile. Log in to save your preferences.</p>
        <button
          @click="handleOidcLogin"
          class="w-full px-4 py-2 bg-blue-500 text-white rounded-md hover:bg-blue-600 transition-colors mb-3"
        >
          Log In with SSO
        </button>
        <button
          @click="showLoginForm = true"
          class="w-full px-4 py-2 border border-gray-300 rounded-md text-gray-700 hover:bg-gray-50 transition-colors mb-3"
        >
          Legacy Login (Dev Only)
        </button>
        <button
          @click="emit('close')"
          class="w-full px-4 py-2 border border-gray-300 rounded-md text-gray-700 hover:bg-gray-50 transition-colors"
        >
          Close
        </button>
      </div>

      <!-- Login form (fake login) -->
      <div v-else-if="showLoginForm">
        <form @submit.prevent="handleLogin">
          <div class="space-y-4">
            <div>
              <label class="block text-sm font-medium text-gray-700 mb-1">User ID</label>
              <input
                v-model="loginUserId"
                type="text"
                required
                class="w-full px-3 py-2 border border-gray-300 rounded-md focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                placeholder="Enter your user ID"
              />
            </div>
            <div>
              <label class="block text-sm font-medium text-gray-700 mb-1">Name</label>
              <input
                v-model="loginName"
                type="text"
                required
                class="w-full px-3 py-2 border border-gray-300 rounded-md focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                placeholder="Enter your name"
              />
            </div>
          </div>

          <div class="flex space-x-3 mt-6">
            <button
              type="button"
              @click="showLoginForm = false"
              class="flex-1 px-4 py-2 border border-gray-300 rounded-md text-gray-700 hover:bg-gray-50 transition-colors"
            >
              Cancel
            </button>
            <button
              type="submit"
              :disabled="isLoggingIn"
              class="flex-1 px-4 py-2 bg-blue-500 text-white rounded-md hover:bg-blue-600 transition-colors disabled:opacity-50"
            >
              {{ isLoggingIn ? 'Logging in...' : 'Log In' }}
            </button>
          </div>
        </form>
      </div>

      <!-- Authenticated - show profile editor -->
      <form v-else @submit.prevent="saveProfile">
        <div class="mb-4 p-3 bg-blue-50 rounded-md">
          <p class="text-sm text-blue-800">
            Logged in as: <strong>{{ profile.name }}</strong> ({{ profile.userId }})
          </p>
        </div>

        <div class="space-y-4">
          <div>
            <label class="block text-sm font-medium text-gray-700 mb-1">Name</label>
            <input
              v-model="localProfile.name"
              type="text"
              class="w-full px-3 py-2 border border-gray-300 rounded-md focus:ring-2 focus:ring-blue-500 focus:border-transparent"
              placeholder="Enter your name"
            />
          </div>

          <div>
            <label class="block text-sm font-medium text-gray-700 mb-1">Temperature Unit</label>
            <select
              v-model="localProfile.tempUnit"
              class="w-full px-3 py-2 border border-gray-300 rounded-md focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="celsius">Celsius (°C)</option>
              <option value="fahrenheit">Fahrenheit (°F)</option>
            </select>
          </div>

          <div>
            <label class="block text-sm font-medium text-gray-700 mb-1">Wind Speed Unit</label>
            <select
              v-model="localProfile.windUnit"
              class="w-full px-3 py-2 border border-gray-300 rounded-md focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="ms">Meters per second (m/s)</option>
              <option value="kmh">Kilometers per hour (km/h)</option>
              <option value="knots">Knots</option>
            </select>
          </div>

          <div>
            <label class="block text-sm font-medium text-gray-700 mb-1">Default Location</label>
            <input
              v-model="localProfile.defaultLocation"
              type="text"
              class="w-full px-3 py-2 border border-gray-300 rounded-md focus:ring-2 focus:ring-blue-500 focus:border-transparent"
              placeholder="Enter default location"
            />
          </div>
        </div>

        <div class="flex space-x-3 mt-6">
          <button
            type="button"
            @click="handleLogout"
            class="px-4 py-2 border border-red-300 rounded-md text-red-700 hover:bg-red-50 transition-colors"
          >
            Log Out
          </button>
          <button
            type="button"
            @click="emit('close')"
            class="flex-1 px-4 py-2 border border-gray-300 rounded-md text-gray-700 hover:bg-gray-50 transition-colors"
          >
            Cancel
          </button>
          <button
            type="submit"
            class="flex-1 px-4 py-2 bg-blue-500 text-white rounded-md hover:bg-blue-600 transition-colors"
          >
            Save
          </button>
        </div>
      </form>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import type { UserProfile } from '@/types/weather'

interface Props {
  profile: UserProfile
}

const props = defineProps<Props>()

const emit = defineEmits<{
  update: [profile: UserProfile]
  login: [userId: string, name: string, useLegacy: boolean]
  logout: []
  close: []
}>()

// ============================================================================
// STATE
// ============================================================================

const localProfile = ref<UserProfile>({ ...props.profile })
const showLoginForm = ref(false)
const loginUserId = ref('')
const loginName = ref('')
const isLoggingIn = ref(false)

// ============================================================================
// ACTIONS
// ============================================================================

const saveProfile = (): void => {
  emit('update', localProfile.value)
}

const handleOidcLogin = (): void => {
  // Emit login with empty credentials and useLegacy=false - the API client will handle OIDC redirect
  emit('login', '', '', false)
}

const handleLogin = async (): Promise<void> => {
  if (!loginUserId.value || !loginName.value) return
  
  isLoggingIn.value = true
  try {
    emit('login', loginUserId.value, loginName.value, true)
    loginUserId.value = ''
    loginName.value = ''
    showLoginForm.value = false
  } finally {
    isLoggingIn.value = false
  }
}

const handleLogout = (): void => {
  emit('logout')
}
</script>