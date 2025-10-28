<template>
  <div class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
    <div class="bg-white rounded-lg p-6 w-full max-w-md mx-4">
      <h3 class="text-lg font-semibold mb-4">User Profile</h3>
      
      <form @submit.prevent="saveProfile">
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
  close: []
}>()

// ============================================================================
// STATE - Local copy of profile for editing
// ============================================================================

const editableProfile = ref<UserProfile>({ ...props.profile })

// ============================================================================
// ACTIONS
// ============================================================================

const submitProfileChanges = (): void => {
  emit('update', editableProfile.value)
}

const closeModalWithoutSaving = (): void => {
  emit('close')
}

// ============================================================================
// LEGACY ALIASES (for template compatibility)
// ============================================================================

const localProfile = editableProfile
const saveProfile = submitProfileChanges
</script>